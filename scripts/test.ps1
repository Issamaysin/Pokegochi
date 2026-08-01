$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$zig = Join-Path $projectRoot '.tools\python312\Lib\site-packages\ziglang\zig.exe'
if (-not (Test-Path $zig)) { throw 'Local Zig compiler is missing.' }
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $projectRoot '.zig-cache-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $projectRoot '.zig-cache'
$out = Join-Path $projectRoot '.test-build'
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $projectRoot
try {
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp test/native/test_pet_state.cpp -o "$out/pet_state.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/pet_state.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/Collection.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/GymSystem.cpp src/game/Economy.cpp test/native/test_battle_collection.cpp -o "$out/battle_collection.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/battle_collection.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Itest/native/stubs -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/Collection.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/services/PersistentSave.cpp test/native/test_persistent_save.cpp -o "$out/persistent_save.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/persistent_save.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Output 'All native tests passed.'
} finally { Pop-Location }
