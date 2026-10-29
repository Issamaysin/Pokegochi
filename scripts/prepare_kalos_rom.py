"""Safely stage the user's Pokemon Y ZIP/CIA and extract its RomFS.

The large source archive is never committed.  This helper writes only below
.generated/research/kalos_roms, records hashes/provenance and refuses ZIP path
traversal or ambiguous multi-ROM archives.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

from kalos_rom_data import DEFAULT_Y_ROMFS, Y_GARC_PATHS, audit_manifest


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_WORK = ROOT / ".generated" / "research" / "kalos_roms"
DEFAULT_CTRTOOL = (
    ROOT / ".generated" / "research" / "tools" / "ctrtool-v0.7" / "ctrtool.exe"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(8 * 1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def find_download() -> Path:
    downloads = Path.home() / "Downloads"
    candidates = sorted(
        (
            path for path in downloads.iterdir()
            if path.is_file()
            and path.suffix.lower() == ".zip"
            and "pokemon" in path.name.lower()
            and ("_y_" in path.name.lower() or " y " in path.name.lower())
        ),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    )
    if not candidates:
        raise FileNotFoundError(
            "Pokemon Y ZIP was not found in Downloads; pass --archive explicitly"
        )
    return candidates[0]


def extract_single_rom(archive: Path, work: Path) -> Path:
    with zipfile.ZipFile(archive) as package:
        members = [
            member for member in package.infolist()
            if not member.is_dir() and Path(member.filename).suffix.lower() in (".cia", ".3ds")
        ]
        if len(members) != 1:
            names = ", ".join(member.filename for member in members) or "none"
            raise ValueError(f"Expected exactly one CIA/3DS in ZIP; found: {names}")
        member = members[0]
        member_path = Path(member.filename.replace("\\", "/"))
        if member_path.is_absolute() or ".." in member_path.parts:
            raise ValueError(f"Unsafe ZIP member path: {member.filename}")
        source_dir = work / "source"
        source_dir.mkdir(parents=True, exist_ok=True)
        output = source_dir / Path(member.filename).name
        if not output.is_file() or output.stat().st_size != member.file_size:
            temporary = output.with_suffix(output.suffix + ".partial")
            with package.open(member) as compressed, temporary.open("wb") as extracted:
                shutil.copyfileobj(compressed, extracted, length=8 * 1024 * 1024)
            if temporary.stat().st_size != member.file_size:
                raise IOError("Extracted ROM size does not match ZIP central directory")
            os.replace(temporary, output)
        return output


def ctrtool_path(configured: str | None) -> Path:
    if configured:
        candidate = Path(configured).expanduser()
    elif os.environ.get("POKEGOCHI_CTRTOOL"):
        candidate = Path(os.environ["POKEGOCHI_CTRTOOL"]).expanduser()
    else:
        candidate = DEFAULT_CTRTOOL
    if not candidate.is_file():
        from_path = shutil.which("ctrtool")
        if from_path:
            return Path(from_path)
        raise FileNotFoundError(
            f"ctrtool not found at {candidate}; pass --ctrtool or set POKEGOCHI_CTRTOOL"
        )
    return candidate


def run(command: list[str], label: str) -> str:
    result = subprocess.run(command, text=True, capture_output=True)
    output = "\n".join(part.strip() for part in (result.stdout, result.stderr) if part.strip())
    if result.returncode:
        tail = "\n".join(output.splitlines()[-30:])
        raise RuntimeError(f"{label} failed with exit code {result.returncode}:\n{tail}")
    return output


def extract_romfs(rom: Path, tool: Path, work: Path) -> Path:
    romfs = DEFAULT_Y_ROMFS
    expected = romfs / Y_GARC_PATHS["personal"]
    if expected.is_file():
        return romfs
    romfs.mkdir(parents=True, exist_ok=True)

    if rom.suffix.lower() == ".3ds":
        run([str(tool), "-x", f"--romfsdir={romfs}", str(rom)], "3DS RomFS extraction")
    else:
        contents = work / "contents"
        contents.mkdir(parents=True, exist_ok=True)
        prefix = contents / "content"
        run([str(tool), "-x", f"--contents={prefix}", str(rom)], "CIA content extraction")
        candidates = sorted(
            (path for path in contents.iterdir() if path.is_file()),
            key=lambda path: path.stat().st_size,
            reverse=True,
        )
        failures: list[str] = []
        for content in candidates:
            try:
                run(
                    [str(tool), "-x", f"--romfsdir={romfs}", str(content)],
                    f"RomFS extraction from {content.name}",
                )
            except RuntimeError as error:
                failures.append(str(error))
                continue
            if expected.is_file():
                break
        if not expected.is_file():
            detail = "\n\n".join(failures[-2:])
            raise RuntimeError(
                "No Pokemon Y RomFS was extracted from the CIA contents. "
                "The CIA may still be encrypted and require the user's own 3DS keys/decrypted dump."
                + (f"\n{detail}" if detail else "")
            )
    if not expected.is_file():
        raise RuntimeError(f"Extraction completed without required Pokemon Y file {expected}")
    return romfs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, help="Pokemon Y ZIP from Google Drive")
    parser.add_argument("--ctrtool", help="Path to ctrtool.exe")
    parser.add_argument("--work", type=Path, default=DEFAULT_WORK)
    args = parser.parse_args()

    archive = (args.archive or find_download()).expanduser().resolve()
    if not archive.is_file():
        raise FileNotFoundError(archive)
    work = args.work.expanduser().resolve()
    work.mkdir(parents=True, exist_ok=True)
    rom = extract_single_rom(archive, work)
    tool = ctrtool_path(args.ctrtool)
    romfs = extract_romfs(rom, tool, work)
    os.environ["POKEGOCHI_Y_ROMFS"] = str(romfs)
    audit = audit_manifest()
    manifest = {
        "archive": str(archive),
        "archive_size": archive.stat().st_size,
        "archive_sha256": sha256(archive),
        "rom": str(rom),
        "rom_size": rom.stat().st_size,
        "rom_sha256": sha256(rom),
        "ctrtool": str(tool),
        "tables": audit,
    }
    manifest_path = work / "source_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError, RuntimeError, zipfile.BadZipFile) as error:
        print(f"prepare_kalos_rom: {error}", file=sys.stderr)
        raise SystemExit(1)
