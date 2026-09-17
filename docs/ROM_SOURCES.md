# Local ROM source manifest

These files are user-supplied, remain local, and are excluded from source control.

| Region source | File | SHA-1 | Purpose |
|---|---|---|---|
| Johto | Pokemon Gold (USA, Europe), 2 MiB | `D8B8A3600A465308C9953DFA04F0081C05BDCB94` | Progression, encounters, trainers, Gyms, teams, levels, dialogue, Badges |
| Hoenn | Pokemon Emerald (USA, Europe), 16 MiB | `F3AE088181BF583E55DAF962A92BB46F4F1D07B7` | Progression, encounters, trainers, Gyms, teams, levels, dialogue, visual references |

FireRed remains the canonical battle and Pokemon-data source for all three regions.
Gold's Generation II mechanics must never overwrite FireRed mechanics.
The pokegold data project supplies Gold's progression tables. The pokecrystal
graphics project is used only as an extractable, layout-compatible source for
Johto trainer and Badge pixels, recolored for the FireRed-style UI.
