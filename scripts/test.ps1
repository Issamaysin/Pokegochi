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
    & $python (Join-Path $PSScriptRoot 'generate_server_battle_bot.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_move_category_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_pokedex_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_trainer_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_battle_tower_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $PSScriptRoot 'generate_battle_animation_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_geomancy_animation_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_lava_plume_animation_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_giga_impact_animation_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_battle_animation_flash_safety_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_gen5_animation_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_gen6_animation_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_gen7_animation_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_android_asset_catalog_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_kalos_asset_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_alola_asset_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_alolan_identity_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_ash_bond_presentation_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_rayquaza_presentation_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_android_legendary_box_fixture_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_ultra_beast_legendary_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_gen5_accuracy_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_kalos_rom_data.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_alola_rom_data.py')
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
    & $python (Join-Path $projectRoot 'test\test_android_updater_current_format_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_usb_flash_layout.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_asset_renderer_archive_paths.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_renderer_capacity_recovery_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_wifi_trade_box_sprite_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_ble_trade_bag_regression.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_ui_performance_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_kids_games_removed_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_release_identity_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_wild_auto_battle_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_theme_selector_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_language_selector_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_league_complete_render_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_region_hub_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_region_completion_reward_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_gen5_mega_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_gen6_mega_form_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_complete_mega_catalog_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_form_lifecycle_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_spinda_spots_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_spinda_box_fixture_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_unova_form_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_sinnoh_chain_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_evolution_progression_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_first_master_ball_surprise_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_ble_transport_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_wifi_multiplayer_setup_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_multiplayer_invite_authority_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_pvp_faint_online_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_pvp_turn_deadline_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $python (Join-Path $projectRoot 'test\test_pvp_prompt_transition_contract.py')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_wireless_update_protocol.cpp -o "$out/wireless_update_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/wireless_update_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_multiplayer_protocol.cpp -o "$out/multiplayer_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/multiplayer_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_online_multiplayer_protocol.cpp -o "$out/online_multiplayer_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/online_multiplayer_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude -Iserver/PokegochiOnlineServer/include server/PokegochiOnlineServer/src/ServerBattleBot.cpp test/native/test_server_battle_bot.cpp -o "$out/server_battle_bot.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/server_battle_bot.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude -Iserver/PokegochiOnlineServer/include src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleAnimationData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/PokedexRewards.cpp server/PokegochiOnlineServer/src/ServerBattleBot.cpp test/native/test_server_bot_full_battles.cpp -o "$out/server_bot_full_battles.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/server_bot_full_battles.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_recovery_update_protocol.cpp -o "$out/recovery_update_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/recovery_update_protocol.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/Localization.cpp test/native/test_localization.cpp -o "$out/localization.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/localization.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleAnimationData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/PokedexRewards.cpp src/game/GymSystem.cpp src/game/LeagueSystem.cpp src/game/MegaChallengeSystem.cpp src/game/BattleTowerSystem.cpp src/game/BossSystem.cpp src/game/Economy.cpp src/game/EggSystem.cpp src/services/Multiplayer.cpp test/native/test_multiplayer_session.cpp -o "$out/multiplayer_session.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/multiplayer_session.exe"
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
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_battle_hp_animation.cpp -o "$out/battle_hp_animation.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/battle_hp_animation.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_battle_presentation_timing.cpp -o "$out/battle_presentation_timing.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/battle_presentation_timing.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/PartyPresets.cpp test/native/test_party_presets.cpp -o "$out/party_presets.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/party_presets.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude test/native/test_touch_filter.cpp -o "$out/touch_filter.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/touch_filter.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleAnimationData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/PokedexRewards.cpp src/game/GymSystem.cpp src/game/LeagueSystem.cpp src/game/MegaChallengeSystem.cpp src/game/BattleTowerSystem.cpp src/game/BossSystem.cpp src/game/Economy.cpp src/game/EggSystem.cpp test/native/test_battle_collection.cpp -o "$out/battle_collection.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/battle_collection.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/GymSystem.cpp src/game/MegaChallengeSystem.cpp src/game/SinnohExpedition.cpp test/native/test_sinnoh_expedition.cpp -o "$out/sinnoh_expedition.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/sinnoh_expedition.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp test/native/test_move_effect_matrix.cpp -o "$out/move_effect_matrix.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/move_effect_matrix.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp test/native/test_gen5_move_sweep.cpp -o "$out/gen5_move_sweep.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/gen5_move_sweep.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp test/native/test_gen6_move_sweep.cpp -o "$out/gen6_move_sweep.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/gen6_move_sweep.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleAnimationData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp test/native/test_gen7_move_sweep.cpp -o "$out/gen7_move_sweep.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/gen7_move_sweep.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/GymSystem.cpp src/game/EggSystem.cpp test/native/test_ability_matrix.cpp -o "$out/ability_matrix.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/ability_matrix.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp test/native/audit_specialized_abilities.cpp -o "$out/specialized_ability_audit.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/specialized_ability_audit.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Itest/native/stubs -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/PartyPresets.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/services/PersistentSave.cpp test/native/test_persistent_save.cpp -o "$out/persistent_save.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/persistent_save.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Itest/native/stubs -Iinclude src/game/HomeBackgrounds.cpp test/native/test_home_backgrounds.cpp -o "$out/home_backgrounds.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/home_backgrounds.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/WildChainSystem.cpp test/native/test_wild_chain.cpp -o "$out/wild_chain.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/wild_chain.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/GymSystem.cpp src/game/MegaChallengeSystem.cpp src/game/SinnohExpedition.cpp src/game/UnovaAdventure.cpp src/game/UnovaForms.cpp src/game/UnovaTournament.cpp src/game/Economy.cpp test/native/test_unova_adventure.cpp -o "$out/unova_adventure.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/unova_adventure.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleAnimationData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/GymSystem.cpp src/game/MegaChallengeSystem.cpp src/game/SinnohExpedition.cpp src/game/UnovaAdventure.cpp src/game/KalosAdventure.cpp test/native/test_kalos_adventure.cpp -o "$out/kalos_adventure.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/kalos_adventure.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $zig c++ -std=c++17 -w -Iinclude src/game/PetState.cpp src/game/PokemonData.cpp src/game/HeldItems.cpp src/game/MegaEvolution.cpp src/game/AlolaForms.cpp src/game/Collection.cpp src/game/WorldFeatures.cpp src/game/TrainerData.cpp src/game/BattleAnimationData.cpp src/game/BattleEngine.cpp src/game/Pokedex.cpp src/game/GymSystem.cpp src/game/MegaChallengeSystem.cpp src/game/SinnohExpedition.cpp src/game/UnovaAdventure.cpp src/game/KalosAdventure.cpp src/game/AlolaAdventure.cpp src/game/Economy.cpp test/native/test_alola_adventure.cpp -o "$out/alola_adventure.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & "$out/alola_adventure.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Output 'All native tests passed.'
} finally { Pop-Location }
