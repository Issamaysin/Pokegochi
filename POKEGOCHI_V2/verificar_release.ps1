$ErrorActionPreference = 'Stop'
$checksumFile = Join-Path $PSScriptRoot 'SHA256SUMS.txt'
if (-not (Test-Path -LiteralPath $checksumFile)) { throw 'SHA256SUMS.txt não encontrado.' }
$checked = 0
foreach ($line in Get-Content -LiteralPath $checksumFile) {
    if (-not $line.Trim()) { continue }
    if ($line -notmatch '^([0-9A-Fa-f]{64})\s{2}(.+)$') { throw "Linha inválida: $line" }
    $expected = $Matches[1].ToUpperInvariant()
    $relative = $Matches[2].Replace('/', '\')
    $path = Join-Path $PSScriptRoot $relative
    if (-not (Test-Path -LiteralPath $path)) { throw "Arquivo ausente: $relative" }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($actual -ne $expected) { throw "Hash incorreto: $relative" }
    ++$checked
}
Write-Host "$checked arquivos da release V2 foram verificados com sucesso." -ForegroundColor Green
