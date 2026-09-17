param(
    [string]$Port,
    [switch]$BuildOnly,
    [switch]$FreshDevice
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $projectRoot '.tools\python312\python.exe'
$pioPython = Join-Path $projectRoot '.pio-core\penv\Scripts\python.exe'
$platformioLauncher = Join-Path $PSScriptRoot 'platformio_local.py'
$environmentName = 'esp32-2432S028R'
$esptool = Join-Path $projectRoot '.pio-core\packages\tool-esptoolpy\esptool.py'
$partitionTool = Join-Path $projectRoot '.pio-core\packages\framework-arduinoespressif32\tools\gen_esp32part.py'
$mkspiffs = Join-Path $projectRoot '.tools\mkspiffs-0.2.3\mkspiffs-0.2.3-arduino-esp32-win32\mkspiffs.exe'
$targetSpiffsOffset = 0x360000
$targetSpiffsSize = 0xA0000

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
    $lines = & $pioPython $esptool '--chip' 'esp32' '--port' $SerialPort 'read-mac' 2>&1
    $code = $LASTEXITCODE
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
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '460800',
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
    if ($pending) {
        Write-Host "Retomando migracao de save interrompida para a placa $deviceId." -ForegroundColor Yellow
        return $pending
    }
    if ($oldOffset -eq $targetSpiffsOffset -and $oldSize -eq $targetSpiffsSize) {
        Write-Host 'Layout OTA ampliado ja esta instalado; o save permanecera no lugar.' -ForegroundColor DarkGray
        return $null
    }

    Write-Host ("Migrando SPIFFS 0x{0:X}/0x{1:X} -> 0x{2:X}/0x{3:X} antes da gravacao..." -f `
        $oldOffset, $oldSize, $targetSpiffsOffset, $targetSpiffsSize) -ForegroundColor Yellow
    $oldImage = Join-Path $backupDir 'spiffs-before.bin'
    $filesDir = Join-Path $backupDir 'save-files'
    $newImage = Join-Path $backupDir 'spiffs-migrated.bin'
    New-Item -ItemType Directory -Path $filesDir -Force | Out-Null
    Invoke-Checked 'Backup da particao de save' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '460800',
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
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '460800',
        'write-flash', ('0x{0:X}' -f $targetSpiffsOffset), $Migration.Image)
    $verifyImage = Join-Path $Migration.Backup 'spiffs-after-readback.bin'
    Invoke-Checked 'Verificacao do save migrado' $pioPython @(
        $esptool, '--chip', 'esp32', '--port', $SerialPort, '--baud', '460800',
        'read-flash', ('0x{0:X}' -f $targetSpiffsOffset), ('0x{0:X}' -f $targetSpiffsSize), $verifyImage)
    $expected = (Get-FileHash -LiteralPath $Migration.Image -Algorithm SHA256).Hash
    $actual = (Get-FileHash -LiteralPath $verifyImage -Algorithm SHA256).Hash
    if ($expected -ne $actual) { throw "A verificacao do save migrado falhou. Backup preservado em $($Migration.Backup)." }
    if ($Migration.Marker -and (Test-Path -LiteralPath $Migration.Marker)) {
        Remove-Item -LiteralPath $Migration.Marker -Force
    }
    Write-Host "Save migrado, regravado e verificado. Backup: $($Migration.Backup)" -ForegroundColor Green
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

$env:PLATFORMIO_CORE_DIR = Join-Path $projectRoot '.pio-core'
$env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = 'No'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$arguments = @($platformioLauncher, 'run', '-e', $environmentName)
$saveMigration = $null
if (-not $BuildOnly) {
    $saveMigration = Prepare-SaveLayoutMigration $Port
    $arguments += @('-t', 'upload', '--upload-port', $Port)
    Write-Host "Compilando somente '$environmentName' e gravando em $Port..." -ForegroundColor Cyan
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
    if (-not $BuildOnly) { Restore-MigratedSave $saveMigration $Port }
} finally {
    Pop-Location
}

if ($BuildOnly) {
    Write-Host 'Firmware compilado com sucesso.' -ForegroundColor Green
} else {
    Write-Host "Firmware gravado e verificado com sucesso em $Port." -ForegroundColor Green
}
