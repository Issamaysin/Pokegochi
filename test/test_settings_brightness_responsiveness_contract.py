#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


def brightness_handler(start_marker: str, end_marker: str) -> str:
    start = MAIN.index(start_marker)
    end = MAIN.index(end_marker, start)
    return MAIN[start:end]


decrease = brightness_handler(
    "gameSave.settings.brightnessPercent -= 20;",
    "gameSave.settings.brightnessPercent += 20;",
)
increase = brightness_handler(
    "gameSave.settings.brightnessPercent += 20;",
    "#endif",
)

for name, handler in (("decrease", decrease), ("increase", increase)):
    redraw = handler.index("refreshSettingsBrightness();")
    present = handler.index("presentRetainedFrame();")
    save = handler.index("saveNow();")
    assert redraw < present < save, (
        f"{name} must publish the changed percentage before persistent save"
    )
    assert "backlight.setBrightness(gameSave.settings.brightnessPercent);" in handler

print("PASS: brightness percentage and bar are presented before the slow save commit")
