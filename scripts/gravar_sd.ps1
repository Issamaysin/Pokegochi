param(
    [string]$Drive,
    [switch]$RebuildAssets,
    [switch]$Format,
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $projectRoot '.generated\sdcard'
$sourcePack = Join-Path $sourceRoot 'pokegochi\pokegochi.pak'
$sourceVersion = Join-Path $sourceRoot 'pokegochi\assets\pack_version.txt'
$sourceAnimationCatalog = Join-Path $sourceRoot 'pokegochi\assets\animation_catalog.txt'
$python = Join-Path $projectRoot '.tools\python312\python.exe'
$builder = Join-Path $PSScriptRoot 'build_sd_asset_pack.py'
$projectDrive = ([System.IO.Path]::GetPathRoot($projectRoot)).TrimEnd('\').ToUpperInvariant()

function Find-RemovableDrives {
    return @(Get-CimInstance Win32_LogicalDisk -Filter 'DriveType = 2' -ErrorAction SilentlyContinue |
        Where-Object { $_.DeviceID -and (Test-Path -LiteralPath ($_.DeviceID + '\')) } |
        ForEach-Object { $_.DeviceID.ToUpperInvariant() } |
        Sort-Object -Unique)
}

function Normalize-Drive([string]$Value) {
    $normalized = $Value.Trim().TrimEnd('\').ToUpperInvariant()
    if ($normalized -match '^[A-Z]$') { $normalized += ':' }
    if ($normalized -notmatch '^[A-Z]:$') { throw "Unidade invalida: '$Value'. Use algo como D:." }
    return $normalized
}

if ($Drive) {
    $Drive = Normalize-Drive $Drive
} else {
    $detectedDrives = @(Find-RemovableDrives)
    if ($detectedDrives.Count -eq 1) {
        $Drive = $detectedDrives[0]
    } elseif ($detectedDrives.Count -gt 1) {
        Write-Host "Mais de uma unidade removivel foi encontrada: $($detectedDrives -join ', ')" -ForegroundColor Yellow
        $Drive = Normalize-Drive (Read-Host 'Digite a unidade do microSD (ex.: D:)')
    } else {
        $Drive = Normalize-Drive (Read-Host 'Nenhum microSD foi detectado automaticamente. Digite a unidade (ex.: D:)')
    }
}

if ($Drive -eq $projectDrive -or $Drive -eq 'C:') {
    throw "Recusando usar a unidade $Drive porque ela contem o sistema ou o projeto."
}

$driveRoot = $Drive + '\'
if (-not (Test-Path -LiteralPath $driveRoot)) {
    throw "A unidade $Drive nao esta montada."
}

$logicalDisk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID = '$Drive'" -ErrorAction SilentlyContinue
if (-not $logicalDisk) { throw "Nao foi possivel validar a unidade $Drive." }

if ($Format) {
    if ([int]$logicalDisk.DriveType -ne 2) {
        throw "A unidade $Drive nao foi identificada como removivel. A formatacao foi bloqueada por seguranca."
    }
    if (-not $Yes) {
        Write-Host "ATENCAO: todos os arquivos de $Drive serao apagados." -ForegroundColor Red
        $confirmation = (Read-Host "Digite exatamente $Drive para confirmar").Trim().ToUpperInvariant()
        if ($confirmation -ne $Drive) { throw 'Formatacao cancelada.' }
    }
    Write-Host "Formatando $Drive como FAT32..." -ForegroundColor Yellow
    Format-Volume -DriveLetter $Drive[0] -FileSystem FAT32 -NewFileSystemLabel 'POKEGOCHI' -Force -Confirm:$false | Out-Null
    Start-Sleep -Milliseconds 800
    if (-not (Test-Path -LiteralPath $driveRoot)) { throw "A unidade $Drive nao voltou a montar apos a formatacao." }
}

if ($RebuildAssets -or -not (Test-Path -LiteralPath $sourcePack)) {
    if (-not (Test-Path -LiteralPath $python)) { throw "Toolchain Python local nao encontrada em '$python'." }
    if (-not (Test-Path -LiteralPath $builder)) { throw "Gerador de assets nao encontrado em '$builder'." }
    Write-Host 'Reconstruindo o pacote de assets do SD...' -ForegroundColor Cyan
    Push-Location $projectRoot
    try {
        $env:PYTHONUTF8 = '1'
        $env:PYTHONIOENCODING = 'utf-8'
        & $python (Join-Path $PSScriptRoot 'generate_battle_animation_data.py')
        if ($LASTEXITCODE -ne 0) { throw "Gerador de animacoes terminou com codigo $LASTEXITCODE." }
        & $python $builder
        if ($LASTEXITCODE -ne 0) { throw "Gerador de assets terminou com codigo $LASTEXITCODE." }
    } finally {
        Pop-Location
    }
} else {
    Write-Host 'Reutilizando o pacote de assets ja gerado. Use -RebuildAssets somente quando os graficos mudarem.' -ForegroundColor DarkGray
}

if (-not (Test-Path -LiteralPath $sourcePack)) {
    throw "Pacote esperado nao encontrado: '$sourcePack'."
}
if (-not (Test-Path -LiteralPath $sourceVersion)) {
    throw "Pacote de assets antigo ou incompleto. Execute com -RebuildAssets: '$sourceVersion'."
}
if (-not (Test-Path -LiteralPath $sourceAnimationCatalog)) {
    throw "Catalogo de animacoes ausente. Execute com -RebuildAssets: '$sourceAnimationCatalog'."
}

$requiredBytes = (Get-ChildItem -LiteralPath $sourceRoot -File -Recurse | Measure-Object Length -Sum).Sum
$logicalDisk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID = '$Drive'"
if ([uint64]$logicalDisk.FreeSpace -lt [uint64]$requiredBytes) {
    throw "Espaco insuficiente em $Drive. Necessario: $([math]::Ceiling($requiredBytes / 1MB)) MB."
}

Write-Host "Copiando o Pokegochi para $Drive (8 fluxos paralelos)..." -ForegroundColor Cyan
# O pacote possui milhares de sprites pequenos. A copia serial passava mais
# tempo criando entradas FAT do que transferindo dados, especialmente nos
# microSD antigos de 256 MB. Oito fluxos mantêm o leitor e o cartão ocupados
# sem a pressão excessiva de fila que /MT:32 provoca nesses dispositivos.
# Robocopy sempre compara tamanho/data antes de escrever, então uma gravação
# interrompida pode ser retomada com segurança pelo mesmo comando.
& robocopy $sourceRoot $driveRoot /E /COPY:DAT /DCOPY:DAT /FFT /R:2 /W:1 /MT:8 /NFL /NDL /NJH /NJS /NP
$robocopyCode = $LASTEXITCODE
if ($robocopyCode -gt 7) { throw "Falha na copia (robocopy codigo $robocopyCode)." }

$destinationPack = Join-Path $driveRoot 'pokegochi\pokegochi.pak'
$destinationVersion = Join-Path $driveRoot 'pokegochi\assets\pack_version.txt'
$destinationAnimationCatalog = Join-Path $driveRoot 'pokegochi\assets\animation_catalog.txt'
if (-not (Test-Path -LiteralPath $destinationPack)) {
    throw "O pacote nao apareceu no destino: '$destinationPack'."
}
if (-not (Test-Path -LiteralPath $destinationVersion) -or
    (Get-Content -LiteralPath $destinationVersion -Raw).Trim() -ne (Get-Content -LiteralPath $sourceVersion -Raw).Trim()) {
    throw 'A versao do pacote de assets nao foi copiada corretamente.'
}
if (-not (Test-Path -LiteralPath $destinationAnimationCatalog) -or
    (Get-Content -LiteralPath $destinationAnimationCatalog -Raw).Trim() -ne
    (Get-Content -LiteralPath $sourceAnimationCatalog -Raw).Trim()) {
    throw 'O catalogo de animacoes nao foi copiado corretamente.'
}

$sourceHash = (Get-FileHash -LiteralPath $sourcePack -Algorithm SHA256).Hash
$destinationHash = (Get-FileHash -LiteralPath $destinationPack -Algorithm SHA256).Hash
if ($sourceHash -ne $destinationHash) {
    throw 'A verificacao SHA-256 falhou. Nao remova o cartao; execute a gravacao novamente.'
}

Write-Host "microSD $Drive gravado e verificado com sucesso." -ForegroundColor Green
Write-Host "Pacote: $([math]::Round((Get-Item -LiteralPath $destinationPack).Length / 1MB, 2)) MB  SHA-256: $($destinationHash.Substring(0, 12))..." -ForegroundColor DarkGray
Write-Host 'Agora ejete o cartao com seguranca pelo Windows.' -ForegroundColor Yellow
# Robocopy uses codes 1-7 for successful copies with differences. Do not leak
# that value through PowerShell as if the whole SD writer had failed.
exit 0
