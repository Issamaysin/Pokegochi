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

## Generation 6 Mega Evolutions and late forms

- Source project: **pokeemerald-expansion**
- Local source directory: `.downloads/pokeemerald-expansion/graphics/pokemon/`
- Mega forms: Chesnaught, Delphox, Greninja, Pyroar, Floette, Meowstic,
  Malamar, Barbaracle, Dragalge, Hawlucha, Zygarde and Diancie
- Other forms: Floette Eternal, Zygarde 10%/Complete and Hoopa Unbound
- Generated assets: `pokemon/forms/mega/{species}/mega/` and
  `pokemon/forms/kalos/{species}/{form}/`

These are the authored GBA sprites and balance data available in the romhack
source used by Pokegochi. The Mega additions include expansion forms that are
not mainline-game forms. Normal/shiny front, back and both icon frames are
exported without generated replacement art.
