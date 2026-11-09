#!/usr/bin/env python3
"""Guard the physical ESP32 flash layout used by the recovery script."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "partitions.csv"
FLASH_SCRIPT = ROOT / "scripts" / "gravar_firmware.ps1"
PLATFORMIO = ROOT / "platformio.ini"


def parse_number(value: str) -> int:
    return int(value.strip(), 0)


def partition_rows() -> dict[str, tuple[int, int]]:
    rows: dict[str, tuple[int, int]] = {}
    for line in PARTITIONS.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        fields = [field.strip() for field in line.split(",")]
        if len(fields) < 5:
            continue
        rows[fields[0]] = (parse_number(fields[3]), parse_number(fields[4]))
    return rows


def main() -> None:
    rows = partition_rows()
    assert rows["nvs"] == (0x9000, 0x5000)
    assert rows["otadata"] == (0xE000, 0x2000)
    assert rows["rescue"] == (0x10000, 0xB0000)
    assert rows["app"] == (0xC0000, 0x310000)
    assert rows["spiffs"] == (0x3D0000, 0x30000)
    assert rows["rescue"][0] % 0x10000 == 0
    assert rows["app"][0] % 0x10000 == 0
    assert rows["rescue"][0] + rows["rescue"][1] == rows["app"][0]
    assert rows["app"][0] + rows["app"][1] == rows["spiffs"][0]
    assert rows["spiffs"][0] + rows["spiffs"][1] == 0x400000

    script = FLASH_SCRIPT.read_text(encoding="utf-8")
    assert "function Write-UsbFirmware" in script
    assert "$targetRecoveryOffset = 0x10000" in script
    assert "$targetMainOffset = 0xC0000" in script
    assert "Verify-FlashedFirmware" in script
    assert "'erase-region', '0xE000', '0x2000'" in script
    assert "$targetRecoverySize = 0xB0000" in script
    assert "$targetMainSize = 0x310000" in script
    assert "function Assert-FirmwareFitsPartitions" in script
    assert "Assert-FirmwareFitsPartitions" in script
    assert "A gravacao foi bloqueada antes de tocar na flash." in script
    assert "$targetSpiffsOffset = 0x3D0000" in script
    assert "$targetSpiffsSize = 0x30000" in script

    platformio = PLATFORMIO.read_text(encoding="utf-8")
    assert "use_newlib_nano.py" not in platformio
    print("USB flash-layout guard passed.")


if __name__ == "__main__":
    main()
