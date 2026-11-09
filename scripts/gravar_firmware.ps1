param(
    [string]$Port,
    [switch]$BuildOnly,
    [switch]$FreshDevice
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $projectRoot '.tools\python312\python.exe'
$platformIoCore = Join-Path $projectRoot '.pio-core'
$pioPython = Join-Path $platformIoCore 'penv\Scripts\python.exe'
# A clean workspace may use PlatformIO's ordinary per-user installation
# instead of the optional project-local core. Select whichever complete core
# is already present; both paths use the same project build directory and the
# same save-preserving flash procedure below.
if (-not (Test-Path -LiteralPath $pioPython)) {
    $userPlatformIoCore = Join-Path $env:USERPROFILE '.platformio'
    $userPlatformIoPython = Join-Path $userPlatformIoCore 'penv\Scripts\python.exe'
    if (Test-Path -LiteralPath $userPlatformIoPython) {
        $platformIoCore = $userPlatformIoCore
        $pioPython = $userPlatformIoPython
    }
}
$platformioLauncher = Join-Path $PSScriptRoot 'platformio_local.py'
$environmentName = 'esp32-2432S028R'
$recoveryEnvironmentName = 'recovery'
$esptool = Join-Path $platformIoCore 'packages\tool-esptoolpy\esptool.py'
$partitionTool = Join-Path $platformIoCore 'packages\framework-arduinoespressif32\tools\gen_esp32part.py'
$mkspiffs = Join-Path $projectRoot '.tools\mkspiffs-0.2.3\mkspiffs-0.2.3-arduino-esp32-win32\mkspiffs.exe'
$targetRecoveryOffset = 0x10000
$targetRecoverySize = 0xB0000
$targetMainOffset = 0xC0000
$targetMainSize = 0x310000
$targetSpiffsOffset = 0x3D0000
$targetSpiffsSize = 0x30000

function Invoke-Checked([string]$Label, [string]$Executable, [string[]]$Arguments) {
    # Several ESP32 utilities write ordinary progress messages to stderr.
    # Windows PowerShell turns those messages into terminating
    # NativeCommandError records while ErrorActionPreference is "Stop", even
    # when the process exits successfully.  Judge native tools by their exit
    # code and keep their combined output visible instead.
    $previousErrorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $lines = & $Executable @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorPreference
    }
    if ($lines) { $lines | ForEach-Object { Write-Host $_ } }
    if ($code -ne 0) { throw "$Label terminou com codigo $code." }
}

function Convert-HexNumber([string]$Value) {
    $clean = $Value.Trim()
    if ($clean.StartsWith('0x', [System.StringComparison]::OrdinalIgnoreCase)) {
        return [Convert]::ToUInt32($clean.Substring(2), 16)
    }
    if ($clean -match '^(\d+)\s*([KkMm])$') {
        $multiplier = if ($Matches[2].ToUpperInvariant() -eq 'M') { 1024 * 1024 } else { 1024 }
        return [uint32]([uint64]$Matches[1] * $multiplier)
    }
    return [Convert]::ToUInt32($clean, 10)
}

function Get-DeviceIdentity([string]$SerialPort) {
    $previousErrorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $lines = & $pioPython $esptool '--chip' 'esp32' '--port' $SerialPort 'read-mac' 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorPreference
    }
    if ($lines) { $lines | ForEach-Object { Write-Host $_ } }
    if ($code -ne 0) { throw "Nao foi possivel identificar a placa em $SerialPort." }
    $joined = $lines -join "`n"
    if ($joined -notmatch '(?i)MAC:\s*([0-9a-f]{2}(?::[0-9a-f]{2}){5})') {
        throw "O endereco unico da placa em $SerialPort nao foi encontrado."
    }
    return $Matches[1].Replace(':', '').ToUpperInvariant()
}

function Find-PendingMigration([string]$DeviceId) {
    $backupRoot = Join-Path $projectRoot 'backups'
    if (-not (Test-Path -LiteralPath $backupRoot)) { return $null }
    foreach ($marker in Get-ChildItem -LiteralPath $backupRoot -Recurse -Filter 'migration-pending.json' |
            Sort-Object LastWriteTime -Descending) {
        try {
            $record = Get-Content -LiteralPath $marker.FullName -Raw | ConvertFrom-Json
            if ($record.DeviceId -eq $DeviceId -and
                (Test-Path -LiteralPath $record.Image) -and
                (Get-Item -LiteralPath $record.Image).Length -eq $targetSpiffsSize) {
                return [pscustomobject]@{
                    Image = [string]$record.Image
                    Backup = [string]$record.Backup
                    Marker = $marker.FullName
                    DeviceId = $DeviceId
                }
            }
        } catch {
            Write-Warning "Marcador de migracao invalido ignorado: $($marker.FullName)"
        }
    }
    return $null
}

function Prepare-SaveLayoutMigration([string]$SerialPort) {
    if ($FreshDevice) {
        Write-Host 'Placa marcada como nova: nenhuma particao de save antiga sera migrada.' -ForegroundColor Yellow
        return $null
    }
    foreach ($required in @($pioPython, $esptool, $partitionTool, $mkspiffs)) {
        if (-not (Test-Path -LiteralPath $required)) {
            throw "Ferramenta necessaria para preservar o save nao encontrada: '$required'. Use -FreshDevice somente em uma placa sem save util."
        }
    }
    $deviceId = Get-DeviceIdentity $SerialPort
    $pending = Find-PendingMigration $deviceId
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $backupDir = if ($pending) { $pending.Backup } else {
        Join-Path $projectRoot "backups\ota-layout-$stamp-$deviceId"
    }
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    $tableBin = Join-Path $backupDir 'partition-table-current.bin'
    $tableCsv = Join-Path $backupDir 'partition-table-current.csv'
    Invoke-Checked 'Leitura da tabela de particoes' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
        'read-flash', '0x8000', '0x1000', $tableBin)
    Invoke-Checked 'Leitura da tabela de particoes' $pioPython @($partitionTool, $tableBin, $tableCsv)
    $spiffsFields = $null
    foreach ($line in Get-Content -LiteralPath $tableCsv) {
        if ($line.TrimStart().StartsWith('#')) { continue }
        $fields = @($line.Split(',') | ForEach-Object { $_.Trim() })
        if ($fields.Count -ge 5 -and $fields[0] -eq 'spiffs') { $spiffsFields = $fields; break }
    }
    if (-not $spiffsFields) {
        throw "A placa nao possui uma particao SPIFFS reconhecivel. Use -FreshDevice apenas se nao houver save para preservar. Backup: $backupDir"
    }
    $oldOffset = Convert-HexNumber $spiffsFields[3]
    $oldSize = Convert-HexNumber $spiffsFields[4]
    $nvsBackup = Join-Path $backupDir 'nvs-before.bin'
    if (-not (Test-Path -LiteralPath $nvsBackup)) {
        Invoke-Checked 'Backup das preferencias NVS' $pioPython @(
            $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
            'read-flash', '0x9000', '0x5000', $nvsBackup)
    }
    if ($pending) {
        Write-Host "Retomando migracao de save interrompida para a placa $deviceId." -ForegroundColor Yellow
        return $pending
    }
    if ($oldOffset -eq $targetSpiffsOffset -and $oldSize -eq $targetSpiffsSize) {
        $saveBackup = Join-Path $backupDir 'spiffs-before.bin'
        Invoke-Checked 'Backup preventivo da particao de save' $pioPython @(
            $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
            'read-flash', ('0x{0:X}' -f $oldOffset), ('0x{0:X}' -f $oldSize), $saveBackup)
        Write-Host "Save e NVS preservados em $backupDir; o SPIFFS permanecera no mesmo endereco." -ForegroundColor DarkGray
        return $null
    }

    Write-Host ("Migrando SPIFFS 0x{0:X}/0x{1:X} -> 0x{2:X}/0x{3:X} antes da gravacao..." -f `
        $oldOffset, $oldSize, $targetSpiffsOffset, $targetSpiffsSize) -ForegroundColor Yellow
    $oldImage = Join-Path $backupDir 'spiffs-before.bin'
    $filesDir = Join-Path $backupDir 'save-files'
    $newImage = Join-Path $backupDir 'spiffs-migrated.bin'
    New-Item -ItemType Directory -Path $filesDir -Force | Out-Null
    Invoke-Checked 'Backup da particao de save' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
        'read-flash', ('0x{0:X}' -f $oldOffset), ('0x{0:X}' -f $oldSize), $oldImage)
    # mkspiffs 0.2.3 on Windows prefixes extraction paths with ".\\" and
    # consequently cannot handle an absolute drive-qualified path. Run it in
    # the backup directory and give it simple relative paths.
    Push-Location $backupDir
    try {
        Invoke-Checked 'Extracao do save SPIFFS' $mkspiffs @(
            '-u', 'save-files', '-b', '4096', '-p', '256', '-s', $oldSize.ToString(), 'spiffs-before.bin')
    } finally {
        Pop-Location
    }
    $saveFiles = @(Get-ChildItem -LiteralPath $filesDir -File -Recurse)
    if (-not $saveFiles.Count) {
        throw "A particao antiga foi preservada, mas nenhum arquivo pode ser extraido. Gravacao cancelada. Backup: $backupDir"
    }
    Push-Location $backupDir
    try {
        Invoke-Checked 'Criacao do save no novo layout' $mkspiffs @(
            '-c', 'save-files', '-b', '4096', '-p', '256', '-s', $targetSpiffsSize.ToString(), 'spiffs-migrated.bin')
    } finally {
        Pop-Location
    }
    $marker = Join-Path $backupDir 'migration-pending.json'
    [pscustomobject]@{
        DeviceId = $deviceId
        Image = $newImage
        Backup = $backupDir
        TargetOffset = ('0x{0:X}' -f $targetSpiffsOffset)
        TargetSize = ('0x{0:X}' -f $targetSpiffsSize)
    } | ConvertTo-Json | Set-Content -LiteralPath $marker -Encoding UTF8
    return [pscustomobject]@{
        Image = $newImage
        Backup = $backupDir
        Marker = $marker
        DeviceId = $deviceId
    }
}

function Restore-MigratedSave($Migration, [string]$SerialPort) {
    if (-not $Migration) { return }
    Invoke-Checked 'Gravacao do save migrado' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
        'write-flash', ('0x{0:X}' -f $targetSpiffsOffset), $Migration.Image)
    $verifyImage = Join-Path $Migration.Backup 'spiffs-after-readback.bin'
    Invoke-Checked 'Verificacao do save migrado' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
        'read-flash', ('0x{0:X}' -f $targetSpiffsOffset), ('0x{0:X}' -f $targetSpiffsSize), $verifyImage)
    $expected = (Get-FileHash -LiteralPath $Migration.Image -Algorithm SHA256).Hash
    $actual = (Get-FileHash -LiteralPath $verifyImage -Algorithm SHA256).Hash
    if ($expected -ne $actual) { throw "A verificacao do save migrado falhou. Backup preservado em $($Migration.Backup)." }
    if ($Migration.Marker -and (Test-Path -LiteralPath $Migration.Marker)) {
        Remove-Item -LiteralPath $Migration.Marker -Force
    }
    Write-Host "Save migrado, regravado e verificado. Backup: $($Migration.Backup)" -ForegroundColor Green
}

function Reset-OtaBootSelection([string]$SerialPort) {
    # With no OTA record the bootloader enters the immutable factory recovery.
    # Recovery validates the partition table and selects the large main image.
    # NVS and SPIFFS (pairing/settings/save data) remain outside this region.
    Invoke-Checked 'Selecao da nova particao de firmware' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
        '--before', 'default-reset', '--after', 'hard-reset',
        'erase-region', '0xE000', '0x2000')
    Write-Host 'Metadados OTA reiniciados: o recovery validara e iniciara o app principal.' -ForegroundColor Green
}

function Verify-FlashedFirmware([string]$SerialPort) {
    $mainFirmware = Join-Path $projectRoot ".pio\build\$environmentName\firmware.bin"
    $recoveryFirmware = Join-Path $projectRoot ".pio\build\$recoveryEnvironmentName\firmware.bin"
    foreach ($firmware in @($mainFirmware, $recoveryFirmware)) {
        if (-not (Test-Path -LiteralPath $firmware)) {
            throw "Firmware compilado nao encontrado para verificacao: '$firmware'."
        }
    }
    Invoke-Checked 'Verificacao do recovery' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
        'verify-flash', ('0x{0:X}' -f $targetRecoveryOffset), $recoveryFirmware)
    Invoke-Checked 'Verificacao do app principal' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
        'verify-flash', ('0x{0:X}' -f $targetMainOffset), $mainFirmware)
}

function Assert-FirmwareFitsPartitions {
    $images = @(
        [pscustomobject]@{ Label = 'recovery'; Path = Join-Path $projectRoot ".pio\build\$recoveryEnvironmentName\firmware.bin"; Limit = $targetRecoverySize },
        [pscustomobject]@{ Label = 'app principal'; Path = Join-Path $projectRoot ".pio\build\$environmentName\firmware.bin"; Limit = $targetMainSize }
    )
    foreach ($image in $images) {
        if (-not (Test-Path -LiteralPath $image.Path)) {
            throw "Firmware compilado nao encontrado: '$($image.Path)'."
        }
        $firmwareBytes = (Get-Item -LiteralPath $image.Path).Length
        if ($firmwareBytes -gt $image.Limit) {
            throw (("Imagem {0} possui {1} bytes e excede sua particao de {2} bytes por {3} bytes. " +
                    "A gravacao foi bloqueada antes de tocar na flash.") -f
                    $image.Label, $firmwareBytes, $image.Limit,
                    ($firmwareBytes - $image.Limit))
        }
        Write-Host ("Imagem {0} validada: {1}/{2} bytes ({3} bytes livres)." -f
            $image.Label, $firmwareBytes, $image.Limit,
            ($image.Limit - $firmwareBytes)) -ForegroundColor DarkGray
    }
}

function Write-UsbFirmware([string]$SerialPort) {
    $buildDir = Join-Path $projectRoot ".pio\build\$environmentName"
    $bootloader = Join-Path $buildDir 'bootloader.bin'
    $partitions = Join-Path $buildDir 'partitions.bin'
    $mainFirmware = Join-Path $buildDir 'firmware.bin'
    $recoveryFirmware = Join-Path $projectRoot ".pio\build\$recoveryEnvironmentName\firmware.bin"
    $bootApp0 = Join-Path $platformIoCore 'packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin'
    foreach ($required in @($bootloader, $partitions, $bootApp0, $recoveryFirmware, $mainFirmware)) {
        if (-not (Test-Path -LiteralPath $required)) {
            throw "Imagem necessaria para gravacao USB nao encontrada: '$required'."
        }
    }
    # Explicit addresses are mandatory: PlatformIO's generic uploader chooses
    # only one app image, while this layout always installs both the immutable
    # recovery and the larger game image without touching NVS or SPIFFS.
    Invoke-Checked 'Gravacao do recovery, app e tabela de particoes' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '115200',
        '--before', 'default-reset', '--after', 'hard-reset', 'write-flash', '-z',
        '--flash-mode', 'dio', '--flash-freq', '40m', '--flash-size', 'detect',
        '0x1000', $bootloader,
        '0x8000', $partitions,
        '0xe000', $bootApp0,
        ('0x{0:X}' -f $targetRecoveryOffset), $recoveryFirmware,
        ('0x{0:X}' -f $targetMainOffset), $mainFirmware)
}

function Find-Esp32Ports {
    $ports = @()
    try {
        $ports = Get-PnpDevice -PresentOnly -Class Ports -ErrorAction Stop |
            Where-Object {
                $_.FriendlyName -match 'CH340|CH341|CP210|USB.?SERIAL|USB Serial|ESP32' -and
                $_.FriendlyName -match '\(COM\d+\)'
            } |
            ForEach-Object {
                if ($_.FriendlyName -match '\((COM\d+)\)') { $Matches[1].ToUpperInvariant() }
            }
    } catch {
        $ports = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
            Where-Object { $_.Description -match 'CH340|CH341|CP210|USB.?SERIAL|ESP32' } |
            ForEach-Object { $_.DeviceID.ToUpperInvariant() }
    }
    return @($ports | Sort-Object -Unique)
}

if (-not (Test-Path -LiteralPath $python)) {
    throw "Toolchain local nao encontrada em '$python'."
}
if (-not (Test-Path -LiteralPath $platformioLauncher)) {
    throw "Launcher do PlatformIO nao encontrado em '$platformioLauncher'."
}

if (-not $BuildOnly) {
    if ($Port) {
        $Port = $Port.ToUpperInvariant()
        if ($Port -notmatch '^COM\d+$') { throw "Porta invalida: '$Port'. Use algo como COM6." }
    } else {
        $detectedPorts = @(Find-Esp32Ports)
        if ($detectedPorts.Count -eq 1) {
            $Port = $detectedPorts[0]
        } elseif ($detectedPorts.Count -gt 1) {
            Write-Host "Mais de uma placa/porta USB serial foi encontrada: $($detectedPorts -join ', ')" -ForegroundColor Yellow
            $Port = (Read-Host 'Digite a porta da placa que deseja gravar (ex.: COM6)').Trim().ToUpperInvariant()
            if ($Port -notmatch '^COM\d+$' -or $detectedPorts -notcontains $Port) {
                throw "A porta '$Port' nao esta entre as portas detectadas."
            }
        } else {
            $Port = (Read-Host 'Nenhuma placa foi detectada automaticamente. Digite a porta (ex.: COM6)').Trim().ToUpperInvariant()
            if ($Port -notmatch '^COM\d+$') { throw "Porta invalida: '$Port'." }
        }
    }
}

$env:PLATFORMIO_CORE_DIR = $platformIoCore
$env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = 'No'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$arguments = @($platformioLauncher, 'run', '-e', $environmentName)
$recoveryArguments = @($platformioLauncher, 'run', '-e', $recoveryEnvironmentName)
$saveMigration = $null
if (-not $BuildOnly) {
    $saveMigration = Prepare-SaveLayoutMigration $Port
    Write-Host "Compilando '$environmentName' e gravando a app0 em $Port..." -ForegroundColor Cyan
    Write-Host 'O save persistente nao sera apagado.' -ForegroundColor DarkGray
} else {
    Write-Host "Compilando somente '$environmentName' (sem gravar)..." -ForegroundColor Cyan
}

Push-Location $projectRoot
try {
    # Ensure the firmware descriptor table and the SD animation catalog come
    # from the same FireRed scripts before compiling/uploading.
    & $python (Join-Path $PSScriptRoot 'generate_battle_animation_data.py')
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao gerar o catalogo de animacoes.' }
    & $python (Join-Path $PSScriptRoot 'generate_tmhm_data.py')
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao gerar a compatibilidade de TMs/HMs do Emerald.' }
    & $python (Join-Path $PSScriptRoot 'generate_battle_tower_data.py')
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao gerar os treinadores da Battle Tower do Emerald.' }
    & $python @arguments
    if ($LASTEXITCODE -ne 0) { throw "PlatformIO terminou com codigo $LASTEXITCODE." }
    & $python @recoveryArguments
    if ($LASTEXITCODE -ne 0) { throw "Build do recovery terminou com codigo $LASTEXITCODE." }
    Assert-FirmwareFitsPartitions
    if (-not $BuildOnly) {
        Write-UsbFirmware $Port
        Restore-MigratedSave $saveMigration $Port
        Verify-FlashedFirmware $Port
        Reset-OtaBootSelection $Port
    }
} finally {
    Pop-Location
}

if ($BuildOnly) {
    Write-Host 'Firmware compilado com sucesso.' -ForegroundColor Green
} else {
    Write-Host "Firmware gravado e verificado com sucesso em $Port." -ForegroundColor Green
}
