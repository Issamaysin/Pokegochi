$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $projectRoot '.tools\python312\python.exe'

if (-not (Test-Path $python)) {
    throw 'Local Python toolchain is missing. See README.md.'
}

$env:PLATFORMIO_CORE_DIR = Join-Path $projectRoot '.pio-core'
$env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = 'No'
$env:PYTHONUTF8 = '1'

Push-Location $projectRoot
try {
    & $python (Join-Path $PSScriptRoot 'platformio_local.py') run
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}
