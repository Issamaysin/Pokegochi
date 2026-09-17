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
    # Regenerate and stamp the move-animation catalog before PlatformIO scans
    # dependencies. Without the stamp, Windows builds could link a stale
    # BattleAnimationData object while the SD already contained newer frames.
    & $python (Join-Path $PSScriptRoot 'generate_battle_animation_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_tmhm_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_battle_tower_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    # Build only the production board.  Running without -e also builds the
    # diagnostic sd-stress environment and needlessly doubles routine builds.
    & $python (Join-Path $PSScriptRoot 'platformio_local.py') run -e esp32-2432S028R
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}
