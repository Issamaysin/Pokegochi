# FireRed self/side move audit

Source of truth: `gMoves` from the local `pokefirered` reference. The generated
firmware table contains 67 moves whose original target is `MOVE_TARGET_USER`.
The target byte is retained as `MoveTarget::User`; these moves never roll
against the opponent's accuracy/evasion, Soundproof-style interception or
Protect. Native tests execute all 67 from both battle sides and reject any
opponent-directed `MoveMissed` event.

## Native now

- Stat/setup: Swords Dance, Growth, Meditate, Agility, Harden, Minimize,
  Withdraw, Barrier, Amnesia, Acid Armor, Sharpen, Tail Glow, Iron Defense,
  Howl, Bulk Up, Calm Mind, Cosmic Power and Dragon Dance.
- Recovery/cure: Recover, Soft-Boiled, Milk Drink, Rest, Morning Sun,
  Synthesis, Moonlight, Slack Off, Heal Bell, Aromatherapy and Refresh.
- Side/field: Mist, Light Screen, Reflect, Haze, Safeguard, Sandstorm, Rain
  Dance, Sunny Day, Hail, Perish Song, Mud Sport and Water Sport.
- Other: Focus Energy, Substitute, Protect, Detect, Endure and Belly Drum.
- Dedicated Gen-III effects: Bide, Teleport, Conversion, Conversion 2,
  Destiny Bond, Baton Pass, Stockpile, Spit Up, Swallow, Charge, Wish,
  Ingrain, Recycle, Imprison, Grudge and Camouflage. Defense Curl also records
  the original hidden flag that doubles Rollout's power.

The side conditions above persist across switches, expire after their original
five turns and are mirrored for player/enemy/PvP. Their visual scope is also
explicit: personal effects animate on the user, screens on the user's side and
weather/Haze across the arena.

## Deliberate single-battle adaptations

- Splash retains its original no-effect result.
- Follow Me and Helping Hand have no legal partner in a one-versus-one battle
  and therefore fail without altering either Pokemon.
- Teleport's overworld destination is absent; its FireRed wild-battle escape,
  trapping failure and trainer-battle failure are native.

## Dedicated-handler completion gate

No self/side-target entry remains on the former missing-handler list. The
dedicated effects above use explicit persistent state and are exercised by
`test/native/test_move_effect_matrix.cpp` from both battle sides. Bide, Baton
Pass, Stockpile/Spit Up/Swallow and Encore retain their additional scenario
tests in `test/native/test_battle_collection.cpp`.

An effect may only remain in this section while its player/enemy matrix covers
prerequisites, duration, switch behavior, failure and event order. Animation
scope remains separately audited by the generated FireRed visual descriptors.

## Subsequent explicit-effect batch

The wider 198-effect audit has also removed the following effects from generic
fallback: Foresight/Odor Sleuth, Spite, Skill Swap, Taunt, Torment, Yawn,
Spikes, Pay Day, Counter, Mirror Coat, Focus Punch, Revenge, Rage, Fury Cutter,
Present, Rampage/Uproar, Hidden Power, Weather Ball, Jump Kick/Hi Jump Kick
crash damage, and every Generation-III two-turn attack (including Skull Bash
and Sky Attack). It now also covers Metronome, Mirror Move, Sleep Talk,
Nature Power, Assist, Mimic, Sketch, Minimize-sensitive hits, Rollout,
Fake Out, Attract, Beat Up, Memento, terrain-dependent Secret Power,
Poison Fang, Poison Tail, Magic Coat, Snatch and Pursuit-before-switch.
Player and enemy paths share the same state and behavioral handlers.

PvP authoritative snapshots include the dedicated move state for every
roster slot plus both Spikes fields. Taunt, Yawn, attraction, move locks,
Recycle, Magic Coat and Snatch therefore cannot disappear or diverge after a
turn is accepted by the host.

`test/native/test_move_effect_matrix.cpp` is the behavioral gate for this
batch. Entries are not described as complete merely because their animation
or generated move row exists.
