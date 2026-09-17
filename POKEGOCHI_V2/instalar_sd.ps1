param(
    [string]$Drive,
    [switch]$Format,
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'
$releaseRoot = $PSScriptRoot
$archive = Join-Path $releaseRoot 'sd\pokegochi-sd-v2.zip'
$releaseDrive = ([IO.Path]::GetPathRoot($releaseRoot)).TrimEnd('\').ToUpperInvariant()

function Normalize-Drive([string]$Value) {
    $normalized = $Value.Trim().TrimEnd('\').ToUpperInvariant()
    if ($normalized -match '^[A-Z]$') { $normalized += ':' }
    if ($normalized -notmatch '^[A-Z]:$') { throw "Unidade inválida: $Value" }
    return $normalized
}

if (-not (Test-Path -LiteralPath $archive)) { throw "Pacote do SD não encontrado: $archive" }
if (-not $Drive) { $Drive = Read-Host 'Digite a unidade do microSD, por exemplo D:' }
$Drive = Normalize-Drive $Drive
if ($Drive -eq 'C:' -or $Drive -eq $releaseDrive) {
    throw "Recusando usar $Drive porque ela contém o sistema ou a release."
}
$root = $Drive + '\'
if (-not (Test-Path -LiteralPath $root)) { throw "A unidade $Drive não está montada." }
$disk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID = '$Drive'"
if (-not $disk) { throw "Não foi possível validar $Drive." }

if ($Format) {
    if ([int]$disk.DriveType -ne 2) { throw "$Drive não foi identificada como removível." }
    if (-not $Yes) {
        Write-Host "ATENÇÃO: todos os arquivos de $Drive serão apagados." -ForegroundColor Red
        if ((Read-Host "Digite exatamente $Drive").Trim().ToUpperInvariant() -ne $Drive) {
            throw 'Formatação cancelada.'
        }
    }
    Format-Volume -DriveLetter $Drive[0] -FileSystem FAT32 -NewFileSystemLabel 'POKEGOCHI' -Force -Confirm:$false | Out-Null
}

$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$tempRoot = [IO.Path]::GetFullPath((Join-Path $tempBase ("PokegochiV2-" + [guid]::NewGuid().ToString('N'))))
if (-not $tempRoot.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'O diretório temporário calculado é inseguro.'
}
New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    Expand-Archive -LiteralPath $archive -DestinationPath $tempRoot -Force
    $sourcePack = Join-Path $tempRoot 'pokegochi\pokegochi.pak'
    if (-not (Test-Path -LiteralPath $sourcePack)) { throw 'O ZIP do SD está incompleto.' }
    & robocopy $tempRoot $root /E /COPY:DAT /DCOPY:DAT /FFT /R:2 /W:1 /MT:4 /NFL /NDL /NJH /NJS /NP
    if ($LASTEXITCODE -gt 7) { throw "Falha na cópia; robocopy código $LASTEXITCODE." }
    $sourceFiles = @(Get-ChildItem -LiteralPath $tempRoot -File -Recurse)
    foreach ($sourceFile in $sourceFiles) {
        $relative = $sourceFile.FullName.Substring($tempRoot.Length).TrimStart('\')
        $destinationFile = Join-Path $root $relative
        if (-not (Test-Path -LiteralPath $destinationFile)) {
            throw "Arquivo não apareceu no cartão: $relative"
        }
        $expected = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash
        $actual = (Get-FileHash -LiteralPath $destinationFile -Algorithm SHA256).Hash
        if ($expected -ne $actual) { throw "Verificação SHA-256 falhou: $relative" }
    }
} finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
Write-Host "microSD $Drive preparado e verificado. Ejete-o com segurança." -ForegroundColor Green
