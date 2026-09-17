param([string]$Output)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $projectRoot '.tools\python312\python.exe'
if (-not $Output) { $Output = Join-Path $projectRoot 'dist\pokegochi-wireless-update.pgota' }
Push-Location $projectRoot
try {
    & $python (Join-Path $PSScriptRoot 'build_wireless_update.py') --output $Output
    if ($LASTEXITCODE -ne 0) { throw "Gerador terminou com codigo $LASTEXITCODE." }
} finally { Pop-Location }
