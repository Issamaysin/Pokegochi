$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$zig = Join-Path $projectRoot '.tools\python312\Lib\site-packages\ziglang\zig.exe'
$python = Join-Path $projectRoot '.tools\python312\python.exe'
if (-not (Test-Path $zig)) { throw 'Local Zig compiler is missing.' }
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $projectRoot '.zig-cache-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $projectRoot '.zig-cache'
$out = Join-Path $projectRoot '.test-build'
New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $projectRoot
try {
    & $python (Join-Path $PSScriptRoot 'generate_pokemon_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_move_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_pokedex_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_trainer_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_battle_tower_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_battle_animation_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_tmhm_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'audit_display_text.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'audit_move_effects.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'audit_abilities.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_wireless_update_package.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_wireless_update_protocol.cpp -o "$out/wireless_update_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/wireless_update_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/services/WirelessUpdate.cpp test/native/test_wireless_update_state_machine.cpp -o "$out/wireless_update_state_machine.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/wireless_update_state_machine.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp test/native/test_pet_state.cpp -o "$out/pet_state.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/pet_state.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_swipe_unlock.cpp -o "$out/swipe_unlock.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/swipe_unlock.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_battle_touch.cpp -o "$out/battle_touch.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/battle_touch.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_touch_filter.cpp -o "$out/touch_filter.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/touch_filter.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleAnimationData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/PokedexRewards.cpp src/game/GymSystem.cpp src/game/LeagueSystem.cpp src/game/MegaChallengeSystem.cpp src/game/BattleTowerSystem.cpp src/game/Economy.cpp src/game/EggSystem.cpp test/native/test_battle_collection.cpp -o "$out/battle_collection.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/battle_collection.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp test/native/test_move_effect_matrix.cpp -o "$out/move_effect_matrix.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/move_effect_matrix.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/GymSystem.cpp src/game/EggSystem.cpp test/native/test_ability_matrix.cpp -o "$out/ability_matrix.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/ability_matrix.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Itest/native/stubs -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/services/PersistentSave.cpp test/native/test_persistent_save.cpp -o "$out/persistent_save.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/persistent_save.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Output 'All native tests passed.'
} finally { Pop-Location }
