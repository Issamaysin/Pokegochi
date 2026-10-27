"""Keep every authored Generation-V form aligned across code and assets."""

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ASSET_SCRIPT = (ROOT / "scripts" / "build_sd_asset_pack.py").read_text(encoding="utf-8")
RENDERER = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
GENERATED = (ROOT / "src" / "game" / "PokemonDataGenerated.inc").read_text(
    encoding="utf-8"
)

FORMS = {
    550: ("", "blue_striped"),
    555: ("", "zen"),
    585: ("", "summer", "autumn", "winter"),
    586: ("", "summer", "autumn", "winter"),
    641: ("", "therian"),
    642: ("", "therian"),
    645: ("", "therian"),
    646: ("", "black", "white"),
    647: ("", "resolute"),
    648: ("", "pirouette"),
    649: ("", "douse_drive", "shock_drive", "burn_drive", "chill_drive"),
}

GENERIC_NAMES = {
    550: "BASCULIN",
    555: "DARMANITAN",
    585: "DEERLING",
    586: "SAWSBUCK",
    641: "TORNADUS",
    642: "THUNDURUS",
    645: "LANDORUS",
    647: "KELDEO",
    648: "MELOETTA",
}


def main() -> None:
    summary_start = RENDERER.index("void rebuildSummaryMovePool(")
    summary_end = RENDERER.index("uint8_t drawSummaryScrollableText", summary_start)
    summary_body = RENDERER[summary_start:summary_end]
    assert "CollectionLogic::resolvedForm(pokemon)" in summary_body
    assert "pokemon.form" not in summary_body, (
        "Summary move pool must be keyed and built from the effective form"
    )

    for species, variants in FORMS.items():
        authored = f'({species}, "'
        assert authored in ASSET_SCRIPT, f"missing form family #{species}"
        for variant in variants[1:]:
            assert f'"{variant}"' in ASSET_SCRIPT, (
                f"missing #{species} asset variant {variant}"
            )
        assert f"species == {species}U" in RENDERER, (
            f"renderer does not route form family #{species}"
        )

    for species, name in GENERIC_NAMES.items():
        assert f'{{{species}, "{name}",' in GENERATED, (
            f"#{species} still exposes a base-form suffix as its species name"
        )

    # When a generated SD tree is available, verify every normal/shiny
    # front/back/icon payload. The source-only assertions above still run in
    # lightweight checkouts where the proprietary ROM-derived pack is absent.
    asset_root = (
        ROOT / ".generated" / "sdcard" / "pokegochi" / "assets" /
        "pokemon" / "forms" / "unova"
    )
    checked = 0
    if asset_root.is_dir():
        for species, variants in FORMS.items():
            for form in range(len(variants)):
                for shiny in ("", "shiny"):
                    prefix = asset_root / str(species) / str(form)
                    if shiny:
                        prefix /= shiny
                    for relative in ("front.pkg", "back.pkg", "icons/0.pkg", "icons/1.pkg"):
                        assert (prefix / relative).is_file(), (
                            f"missing #{species} form {form} {shiny or 'normal'} {relative}"
                        )
                        checked += 1
        assert checked == 240

    print(f"Unova form contract passed ({checked or 'source-only'} assets checked).")


if __name__ == "__main__":
    main()
