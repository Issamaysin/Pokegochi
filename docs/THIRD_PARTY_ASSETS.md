# Third-party assets

## Kids Games animal cards

- Source: **Animal sprites** by gameplayarts
- Original page: https://opengameart.org/content/animal-sprites-1
- License: CC0 1.0 / public domain
- Local source download: `.downloads/animalpackss.png` (not distributed)
- Generated firmware data: `src/game/KidsAnimalSpritesGenerated.inc`

Six sprites are cropped and nearest-neighbour resized for the Memory minigame.
No attribution is required by CC0; this record is retained for provenance.

## Sinnoh Gym Leaders

- Source game: Pokémon Platinum
- Local source directory: `.downloads/sinnoh-trainers/` (not distributed)
- Archive reference: `https://archives.bulbagarden.net/wiki/Category:Generation_IV_Trainer_sprites`
- Files: the eight `Spr Pt` leader sprites for Roark, Gardenia, Fantina,
  Maylene, Crasher Wake, Byron, Candice and Volkner
- Generated assets: `trainers/sinnoh_*.pkg`

The 80x80 portraits are preserved at native pixel resolution. The eight badge
cells come directly from the local pokeplatinum decomp's Trainer Case atlas.

## Generation 5 Mega Evolutions

- Source project: **pokeemerald-expansion**
- Local source directory: `.downloads/pokeemerald-expansion/graphics/pokemon/`
- Forms: Mega Emboar, Excadrill, Audino, Scolipede, Scrafty, Eelektross,
  Chandelure and Golurk
- Generated assets: `pokemon/forms/mega/{500,530,531,545,560,604,609,623}/`

Each form exports the authored indexed front/back sprites, both icon frames
and their normal and shiny palettes. No generated replacement art or runtime
recolouring is used.
