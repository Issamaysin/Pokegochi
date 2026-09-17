# Pokegochi game specification

This document is the current source of truth. The in-game language is English.

## Collection and permanent save

- A device has exactly one game. Production firmware never exposes New Game or Reset.
- Development-only factory reset remains compile-time gated.
- The Box stores up to 386 Pokemon and the active Pokegochi team has up to three.
- The player chooses Bulbasaur, Charmander, or Squirtle on the first start.
- Before the first choice, Professor Oak gives a short FireRed-style introduction. The
  selection then takes place over an enlarged recreation of the real table in Oak's Lab.
- Defeating the Champion of Kanto or Johto opens a permanent starter choice before the next
  region is unlocked: Chikorita/Cyndaquil/Totodile, then Treecko/Torchic/Mudkip.
- Each regional hand-off begins with Oak presenting the discovery of Johto or Hoenn species;
  only after that dialogue does the matching laboratory starter selection appear.
- The nine regional starters are exclusive rewards and never occur as wild encounters.
- Important actions are committed immediately to redundant persistent records.

## Evolution

- Normal level evolutions retain their original FireRed levels. Evolutions that originally
  required stones, trading, friendship, or another outside interaction use a deterministic
  level requirement.
- A Pokemon with exactly one eligible result evolves automatically. When it has several
  branches, the game saves a mandatory evolution-choice prompt before continuing. The player
  chooses the result on the device; it cannot be silently replaced with a default evolution.
- Branches from a later region stay unavailable until that region's starter unlocks its
  Pokedex. Eevee offers Vaporeon, Jolteon, and Flareon in Kanto; Espeon and Umbreon are added
  after Johto unlocks. The same regional filter applies to every other branch.
- Tyrogue keeps its level-20 stat comparison: higher Attack becomes Hitmonlee, higher Defense
  becomes Hitmonchan, and equal values become Hitmontop. Evolution preserves personality,
  Nature, IVs, shiny state, moves, PP, and held item.

## Battle categories

The engine distinguishes `Wild`, `Trainer`, `Gym`, `League`, and `PvP`. Capture is accepted
only when the engine state is `Wild`; hiding the Ball button is not the rule.

### Random wild encounter

- Started from Home whenever the player wants; it has no charge or cooldown.
- Contains wild Pokemon only and never grants additional loot.
- The player chooses any healthy selected-team member before it starts.
- The Pokemon can be captured and grants experience when defeated.
- Through team Lv.20, the Wild level ceiling is one below the strongest
  selected partner. From Lv.21 onward it may match that partner's level.
- The full low-to-high range remains available, but its distribution gradually
  leans toward the party reference level: one third of rolls are upper-biased
  from Lv.21-Lv.39 and two thirds from Lv.40 onward. The unbiased remainder
  means low-level species can still appear at every stage.

### Player-initiated trainer battle

- Consumes one of three VS Seeker charges. Every spent charge rolls its own
  recovery interval between 30 and 60 minutes; partial progress is retained
  while other charges remain available.
- The player chooses any healthy selected-team member before it starts.
- The trainer owns one, two, or three Pokemon and none can be captured.
- Difficulty uses the highest level in the currently selected team.
- It grants experience but no capture farming.
- A generated trainer retains a FireRed trainer identity/class. Its original front
  sprite, encounter presentation, and original encounter line are shown before combat.

## Gyms

- Gyms use fixed FireRed-inspired levels and never scale with the player.
- Every Gym battle has exactly three opponents. Leaders with larger original teams
  keep their three highest-level Pokemon; Brock and Misty are completed with the
  strongest suitable challenger Pokemon from their own FireRed Gym.
- They unlock and appear in canonical order.
- The next Gym appears deterministically when the strongest Pokemon in the
  selected Party reaches that Leader's highest team level. There is no random
  roll or play-time wait.
- Losing leaves the same gym available for another attempt.
- Winning permanently records its Badge and enables the next gym.
- A completed gym never appears again.
- Once its level requirement is met, the Gym remains available from the Home
  screen until victory; choosing `LATER`, losing, or powering off cannot discard it.
- The Trainer Card lives in the Bag's Key Items pocket and displays all eight
  original Kanto Badges, money, Badge count, and the next Gym target.
- Gym invitations show the Leader, Badge reward, fixed recommended level range,
  and the three-battle warning before the player commits. They never consume a
  VS Seeker charge.

Gym content and level gating remain separate from the generic battle engine.

## Pokemon League

- Earning all eight Badges in the current region permanently unlocks its League.
- A League attempt is five consecutive battles: the four original Elite Four
  members followed by the original Champion.
- Every opponent uses three Pokemon selected from the strongest members of their
  original first-clear team, preserving the project's three-Pokemon battle limit.
- Teams and levels are fixed and follow FireRed for Kanto, Gold for Johto, and
  Emerald for Hoenn. Kanto's Champion keeps the original starter counter-pick.
- Elite Four members may use two recovery items and Champions may use four.
- Each victory grants its original-style money reward and shared experience.
- Losing ends the attempt; the next attempt begins again with the first member.
- Defeating the Champion is saved permanently and is required before the next
  regional starter can be chosen.
- The first Kanto League clear also awards the one-and-only Lucky Egg. If any
  selected party Pokemon holds it, the battle's total XP award is increased by
  100% and only then divided equally among every selected party member. The
  holder never receives a larger individual share.
- The Pokedex does not expand when the League is cleared. It expands only after
  the new starter is selected and the Pokedex update message is shown.

## Battle Tower

- The repeatable Battle Tower unlocks after the Hoenn endgame. From then on its
  Home icon is permanently available: there is no invitation roll, cooldown,
  timer, Center dependency, or VS Seeker charge. `Later` merely closes its
  entrance screen and completing or abandoning a run never removes the icon.
- Each run has three consecutive Emerald Battle Tower trainers, with three
  Pokemon per trainer and their authored moves, held items, Natures, IV tiers,
  and EV spreads adapted to Pokegochi's level scaling.
- The entrance screen and Home icon use the actual Emerald Battle Tower map art.
- Completing the third trainer rolls one prize. Rare Candy is the fallback;
  there is a 0.5% chance for a Special Egg when the incubator is free.
- A Special Egg chooses uniformly from the nine regional starters the player
  has never obtained. `seen` is insufficient: only the Pokedex `caught` record
  removes a starter from this pool. Once all nine have been obtained, the Egg
  chooses a random Legendary or Mythical Pokemon from Generations I-III.
- Special Eggs begin incubating immediately and use the rare 96-hour timer.

## Regional source policy

### Sequential generation unlock

- A new save begins as the current 151-Pokemon Kanto game. Only Generation I
  species, Kanto encounters, Kanto trainers, and the eight Kanto Gyms can appear.
- Defeating the eighth Kanto Gym opens the Kanto League. Defeating its Champion
  opens the Johto starter selection. Johto is permanently unlocked only after
  that starter is chosen and the Pokedex update
  message is shown.
  Generation II species then enter encounter progression and the Johto Gyms begin
  appearing in canonical order. Existing Pokemon, party, Box, money, inventory,
  Trainer Card, and Pokédex progress are retained.
- Defeating the eighth Johto Gym opens the Johto League; defeating Champion Lance
  then opens the Hoenn starter selection.
  Hoenn remains unavailable until Treecko, Torchic, or Mudkip is chosen.
  Generation III species then enter encounter progression and the Hoenn Gyms begin
  appearing in canonical order, again retaining all previous progress.
- Unlocks are permanent save milestones and cannot be reset by losing, powering off,
  or changing the active party.
- Previously unlocked generations remain available. Unlocking Johto or Hoenn expands
  the encounter pool; it does not remove earlier-generation Pokemon.
- The Trainer Card presents 24 Badge slots grouped as Kanto, Johto, and Hoenn. Locked
  regions remain hidden or disabled until the previous region is completed.
- Future Pokedex species and page counts do not appear before the corresponding
  regional starter has been selected.
- The Pokédex begins in Kanto mode (001-151), expands to include Johto (001-251), and
  finally becomes the full Generation III National Pokédex (001-386).
- Completing each unlocked generation's visible Pokedex awards exactly one Master Ball:
  Kanto 001-151, Johto 001-251, and Hoenn 001-386. Delivery and acknowledgement are
  persisted separately, so the reward cannot duplicate and its message survives a reboot.

- FireRed remains the single combat-rules authority for every species: base stats,
  types, abilities, experience curves, learnsets, move effects, damage rules,
  evolutions, battle sprites, Box icons, cries, and battle animations.
- Kanto progression, encounters, trainers, Gyms, teams, levels, dialogue, and Badges
  come from FireRed.
- Gold is a Johto content reference only. It supplies encounter progression,
  trainers, Gym order, challengers, leaders, teams, levels, dialogue, and Badges.
  Its Generation II battle rules, learnsets, stats, sprites, and move behavior are
  not imported. Johto content is translated onto the FireRed combat system.
- Emerald supplies Hoenn progression, encounters, trainers, Gyms, teams, levels,
  dialogue, and visual references. Its compatible Generation III data may be used
  for verification, but conflicting Pokemon or battle data still defers to FireRed.
- Gold trainer and Gym presentation may be redrawn or recolored to fit the FireRed
  GBA interface. Pokemon always use their FireRed Generation III assets.
- Original ROM files are local development inputs only and are excluded from source
  control and every release. The private V2 distribution contains only the generated
  microSD package used by the project, never an original ROM image or save dump.

## Bluetooth multiplayer

Multiplayer is a sixth Bag pocket. BLE remains active during the activity-specific
Party/Box selectors, so a player may rearrange the three-member Party or choose a
trade candidate without reconnecting. Actually closing the Bag is the radio lifetime
boundary: it disconnects the peer, stops advertising/scanning, and returns BLE memory
to the graphics renderer. `CANCEL`, the ON/OFF toggle, or a lost physical link also
ends the session.

Each board owns a stable five-digit player ID derived once from its ESP32 identity and
stored separately from the game save. With Bluetooth enabled it advertises a versioned
Pokegochi service. Discovery ignores every advertisement that lacks that service and
Pokegochi manufacturer signature, so phones and unrelated BLE devices never appear in
the player list. Connection, Battle and Trade requests require confirmation on both
devices.

A trade uses the ordinary Box as its selector and requires both players to review the
species and level. The received Pokemon retains all individual data but receives a new
local UID and replaces the outgoing Pokemon in the same Party slot when applicable.
Neither save removes a Pokemon until both peers confirm the same transaction identifier
and payload. Interrupted or mismatched transactions abort. After the durable commit,
both devices play a FireRed-style transfer scene built from the original link-trade
console, wireless-signal, glow and Poke Ball sprites before returning to Multiplayer.

For a link battle, each player uses the connection-preserving Party selector before pressing
`READY`; closing the Bag ends the BLE session. The `READY` press sends a three-Pokemon
roster snapshot. After both players are ready,
each independently chooses the first Pokemon. Battle commands are exchanged in lockstep,
so neither console resolves a turn before both choices are present. The lower five-digit
player ID is the deterministic authority: after the FireRed event sequence for a turn it
sends HP, PP, status, battle-local held-item effects, active slots, weather and RNG state.
The other console applies the snapshot and acknowledges it before either side may choose
again. A dropped notification therefore causes a retransmission instead of divergence.
Link battles reuse the normal battle UI and animations, disable Bag items, grant no XP,
EVs or money, and restore both local Parties exactly when the battle or BLE session ends.

## Care and recovery

- The Pokemon Center stores up to five charges and restores one charge every
  hour whenever its stock is below five.
- One Center charge fully restores HP, PP and persistent combat status.
- The Nurse screen has ten original-map scenes drawn from FireRed and Emerald.
  Each successful healing use randomly selects a different scene for the next
  visit; cancelling does not reroll it and the same scene never repeats twice
  in succession.
- Party Pokemon through Lv.20 recover fully in 10 minutes; from Lv.21 onward
  they recover fully in 30 minutes. Box Pokemon recover fully in three hours. A
  fainted Pokemon remains unavailable until fully healed or revived.

## Individual Pokemon

- Every generated Pokemon receives six permanent IVs from 0 to 31: HP, Attack,
  Defense, Special Attack, Special Defense, and Speed. IVs are generated once and
  survive capture, Box/party movement, level-up, evolution, saving, eggs, and trades.
- The Generation III stat formulas use both IVs and naturally earned EVs. A defeated
  species awards its original effort yield equally to every eligible selected Party
  member. Each stat is capped at 255 EVs and a normal Pokemon at 510 total EVs;
  Pokegochi deliberately grants Shiny Pokemon one additional 255-point total pool.
- One of FireRed's 25 Natures is generated from the permanent personality value.
  A non-neutral Nature raises one stat by 10% and lowers another by 10%; HP is never
  modified. Nature is visible in Summary and the selected Box information panel.
- Ability is the third individual system. It already uses the FireRed species data
  and battle effects; its slot is now selected by the permanent personality bit.
  Evolution preserves personality/Nature/IVs and resolves the evolved species' matching
  Ability slot.
- The fifth Summary page exposes base stats, IVs, and EVs at every level. At Lv.90 it
  also permits redistribution of already-earned EVs without creating new points.
  Breeding inheritance, vitamins, and direct permanent-stat items remain excluded.

## Held items

- Every owned Pokemon can hold exactly one item. The held item is saved with that
  individual and survives level-up, evolution, Box/party movement, and future trades.
- The Bag groups consumable Berries/Herbs separately from permanent held gear. It
  equips items to the currently selected
  Home Pokemon; tapping the target strip takes the equipped item back. Replacing an item
  returns the previous one to the Bag, so swaps cannot duplicate or destroy equipment.
- Consumable held items are Oran, Sitrus, Lum, Persim, Cheri, Chesto, Pecha, Rawst and
  Aspear Berries plus White Herb. They activate automatically and disappear after use.
- Permanent effects include Leftovers, Shell Bell, Choice Band, Quick Claw, Scope Lens,
  BrightPowder, Focus Band, King's Rock, Amulet Coin, Smoke Ball, and all 17 Generation
  III type-boosting items. Effects follow FireRed values where applicable.
- Lucky Egg is a unique first-Kanto-League reward. It never enters Mart stock,
  cannot be duplicated or traded, and gives one party-wide 2x XP multiplier
  before Pokegochi performs its normal equal XP split.
- Thief, Covet, Trick and Knock Off use the held-item state during a battle. In
  wild and NPC battles their transfer/removal is deliberately battle-local, so
  temporary opponents cannot permanently take or delete scarce player gear.
- Poke Ball, Potion, one status medicine and one unowned TM/HM are guaranteed
  Mart families. Its complete twenty-offer stock spans four pages and rotates
  every six hours. Random slots modestly favor held items and may contain
  additional unowned TMs/HMs. Better items enter the pool after their required
  Badge count and rare equipment keeps a lower appearance weight.
- Whenever a random slot would stock Ultra Ball, it has a 0.001% chance to be
  replaced by one Master Ball. That exceptional offer has stock 1 and costs
  99,999; the guaranteed Pokedex and League rewards remain unchanged.
- Vitamins and other direct EV/permanent-stat items are excluded. Lucky Egg is the
  sole bonus-experience exception and is earned through progression.

## Shiny Pokemon

- Every newly generated wild Pokemon and each regional starter independently has
  the original FireRed probability of 1/8192 of being Shiny.
- Random trainers and Gym teams are not assigned random Shiny Pokemon.
- Shiny is a permanent property of the individual Pokemon and survives capture,
  evolution, Box/party movement, saving, and future Bluetooth trades.
- Home, Box, Summary, battle front/back sprites, and evolution use the original
  FireRed Shiny palettes. A Shiny encounter also displays the sparkle cue.
- The Pokedex remains species-only: Shiny forms do not add separate seen/caught
  markers, counters, pages, or completion requirements.

## Pokemon Eggs

- The Day Care unlocks after the third Badge in each region and offers one
  guaranteed regional Egg. Further Eggs have a small chance to be offered after
  trainer victories; only one Egg may be incubated at a time and offers may be refused.
- Incubation lasts 24 hours for common, 48 hours for uncommon, and 96 hours for
  rare Eggs. Normal elapsed device time advances the timer.
- A successful Pokemon Center visit removes 10 minutes and a completed battle removes
  15 minutes. Combined interaction acceleration is capped at two hours per rolling
  24-hour window. Flame Body or Magma Armor in the selected Party doubles normal
  elapsed-time incubation.
- Egg pools contain unlocked-region basic forms only. Starters, evolved forms,
  Legendary and Mythical Pokemon are excluded; baby Pokemon receive extra weight.
- Battle Tower Special Eggs are the sole exception to that normal pool: their
  starter/Legendary contents are selected by the completion reward rules above.
- A hatchling begins at the ace level of the strongest completed Gym, or Lv.5 before
  the first Badge, capped at Lv.89. It uses its normal FireRed ability and learnset,
  may be Shiny at 1/8192, goes to the Box, and marks the species as caught.
- A ready Egg waits if the Box is full. The original FireRed Egg and cracking
  graphics are used for the Home indicator, status screen, and hatch sequence.

## Home backgrounds

The Home scenery is user-selectable from Settings. Thirty-nine native-map
backgrounds are included: fourteen exact 304x134 crops decoded from FireRed maps
and twenty-five from Emerald. The catalogue deliberately avoids near-duplicate
forest and generic-route views: Viridian Forest and Southern Island are the two
forest-style scenes retained, while Pokemon Mansion, Power Plant, Pacifidlog
Town, Shoal Ice Cave, Meteor Falls, Pokemon Tower, and the unique ash-covered
Route 113 provide distinct environments. The Battle Frontier selection includes
its plazas, only two facility lobbies, the Battle Dome arena, Battle Pyramid
plaza, Battle Pike, Battle Arena, and Battle Palace garden. The selected background is persisted in the single game save
and restored after reboot; the backgrounds are never AI-generated, repainted,
or resampled.

Each crop also carries three patrol routes generated from the original map's
collision and metatile-behavior data. Water, currents, lava, walls, trees and
blocked structures are excluded. Lower and upper metatile layers are stored
separately so Pokemon can be occluded by foreground scenery instead of floating
over it. The routes move slowly while retaining the original two-frame PC icon
animation.

## Visual implementation status

- Home, Box, Pokedex, Bag, Mart and battle screens use the approved FireRed-style
  compositions adapted to the 320x240 landscape touchscreen.
- The VS Seeker is reserved for player-initiated trainer encounters; battle commands
  use the original text labels.
- Trainer and Gym Leader introductions use their original FireRed front sprites and
  encounter dialogue. Trainer identity, sprite, dialogue, and team template are one
  catalog entry and must not be randomized independently.
- All 354 moves are generated from their original FireRed animation scripts.
  The ESP32 runtime preserves sprite families, particle counts, cadence, target
  shake, flashes, battler visibility and background-transition intent without a
  full-screen framebuffer. Audio-only commands are intentionally omitted.
- Throwing, opening, shaking, breaking free, and successful capture use the original
  Poke Ball animation family, with the correct ball graphic for each ball type.
- A missing script, untranslated command group, or missing sprite fails generation;
  silent replacement with a generic effect is not allowed.
- Final pixel alignment, touch target sizing, panel color response and animation cadence
  remain part of physical-board visual QA rather than missing game functionality.

## Display power behavior

- No physical button is part of the product. Brightness and the 1/2/5/10-minute
  automatic screen timeout are persistent Display Settings.
- Screen timeout turns off the backlight, sleeps the ILI9341 panel and places the ESP32
  in light sleep. Elapsed-time reconciliation preserves charges, Eggs, passive recovery,
  Mart rotation, friendship, and progression clocks.
- XPT2046 IRQ is the only user wake source. A touch wakes into a dedicated lock screen;
  gameplay remains protected until the player slides the Poke Ball far enough across the
  track. Settings also provides a manual lock button for pocket use.
