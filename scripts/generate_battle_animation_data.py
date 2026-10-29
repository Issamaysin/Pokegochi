"""Translate FireRed move animation scripts into compact ESP32 descriptors.

Unlike the old filename matcher, this generator follows FireRed's actual
ANIM_TAG -> compressed graphics -> palette tables.  It also derives the
canonical OAM cell used by each tag so an atlas is exported as individual
sprites instead of being drawn as one large rectangle on the TFT.
"""
from __future__ import annotations

import json
import hashlib
import re
import ast
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
D = ROOT / ".downloads/pokefirered-tree/pokefirered-master"
EXPANSION = ROOT / ".downloads/pokeemerald-expansion"


def _split_macro_operands(text: str) -> list[str]:
    """Split an assembler macro call without splitting nested expressions."""
    result: list[str] = []
    start = 0
    depth = 0
    for index, character in enumerate(text):
        if character == "(":
            depth += 1
        elif character == ")":
            depth = max(0, depth - 1)
        elif character == "," and depth == 0:
            result.append(text[start:index].strip())
            start = index + 1
    tail = text[start:].strip()
    if tail:
        result.append(tail)
    return result


def _expand_battle_script_macros(script: str, macro_source: Path) -> str:
    """Materialise the expansion's source macros before the VM parser sees them.

    pokeemerald-expansion keeps its authored animation scripts readable with
    wrappers such as ``simple_palette_blend`` and
    ``create_basic_hitsplat_sprite``.  The ROM assembler expands those before
    execution.  Doing the same here is required for the generated Pokegochi
    program to contain the original SpriteTemplate, arguments and timing.
    """
    definitions: dict[str, tuple[list[tuple[str, str | None, bool]], list[str]]] = {}
    macro_text = macro_source.read_text(encoding="utf-8")
    macro_pattern = re.compile(
        r"\.macro\s+([A-Za-z_][A-Za-z0-9_]*)([^\r\n]*)\r?\n"
        r"(.*?)\.endm", re.S)
    for match in macro_pattern.finditer(macro_text):
        parameters: list[tuple[str, str | None, bool]] = []
        for raw in _split_macro_operands(match.group(2).strip()):
            raw = raw.strip()
            if not raw:
                continue
            name = raw.split("=", 1)[0].strip().split(":", 1)[0]
            default = raw.split("=", 1)[1].strip() if "=" in raw else None
            parameters.append((name, default, ":req" in raw))
        definitions[match.group(1)] = (parameters, match.group(3).splitlines())

    def expand_line(line: str, depth: int) -> list[str]:
        if depth > 12:
            raise RuntimeError("Battle animation macro expansion is recursive")
        code = line.split("@", 1)[0].strip()
        if not code:
            return [line]
        parts = code.split(None, 1)
        definition = definitions.get(parts[0])
        # These are real battle-script opcodes.  They also have assembler
        # macro definitions in macros.inc, but expanding those byte encoders
        # would destroy the symbolic script we intentionally feed to the VM.
        if parts[0] in {
                "createsprite", "createvisualtask", "loadspritegfx",
                "unloadspritegfx", "delay", "waitforvisualfinish",
                "monbg", "monbg_static", "clearmonbg", "clearmonbg_static",
                "setalpha", "blendoff", "setbldcnt", "changebg", "fadetobg",
                "restorebg", "splitbgprio", "setarg", "call", "goto",
                "return", "end", "invisible", "visible", "waitbgfadeout",
                "waitbgfadein", "choosetwoturnanim"}:
            definition = None
        if definition is None:
            return [line]
        parameters, body = definition
        actuals = _split_macro_operands(parts[1] if len(parts) > 1 else "")
        values: dict[str, str] = {}
        positional = 0
        for actual in actuals:
            if "=" in actual:
                key, value = actual.split("=", 1)
                values[key.strip()] = value.strip()
            elif positional < len(parameters):
                values[parameters[positional][0]] = actual.strip()
                positional += 1
        for name, default, _required in parameters:
            if name not in values and default is not None:
                values[name] = default
        # These wrappers contain assembler conditionals whose result is a
        # normal visual command. Materialise the selected branch explicitly so
        # the VM receives the same SpriteTemplate/task and argument ABI as the
        # expansion assembler, instead of carrying an opaque macro name.
        if parts[0] == "metallic_shine":
            color = values.get("color")
            return [
                "createvisualtask AnimTask_MetallicShine, " +
                values.get("priority", "5") + ", " +
                values["permanent"] + ", " +
                ("TRUE, " + color if color else "FALSE, RGB_BLACK")
            ]
        if parts[0] in {"createspriteontargets_onpos", "createspriteontargets"}:
            actual = [values.get(name, "") for name, _default, _required in parameters]
            # Both expansion wrappers iterate the selected battle side and
            # replace argv[battlerargindex] for each battler. Pokegochi has a
            # single target per side, so removing only the loop-control index
            # and retaining the authored argv is source-equivalent. This is
            # required by Electroweb's shared ElectricityEffect_OnTargets.
            raw = _split_macro_operands(parts[1] if len(parts) > 1 else "")
            if len(raw) < 4:
                return [line]
            template, battler, priority, battler_arg = raw[:4]
            argv = raw[4:]
            return ["createsprite " + ", ".join(
                [template, battler, priority] + argv)]
        if parts[0] == "create_random_pos_hitsplat_sprite":
            raw = _split_macro_operands(parts[1] if len(parts) > 1 else "")
            positional = [item for item in raw if "=" not in item]
            named = {
                key.strip(): value.strip()
                for item in raw if "=" in item
                for key, value in [item.split("=", 1)]
            }
            if len(positional) < 2 or "relative_to" not in named:
                return [line]
            animation = named.get("animation", "-1")
            return ["createsprite gRandomPosHitSplatSpriteTemplate, " +
                    ", ".join([positional[0], positional[1],
                                named["relative_to"], animation])]
        # This wrapper contains assembler conditionals because it supports
        # both platform and battler shakes.  The emitted createsprite is the
        # same in either branch; only the optional battler selector changes.
        # Materialise that exact result instead of carrying an untranslated
        # macro into the VM audit.
        if parts[0] == "shake_mon_or_platform":
            missing = [name for name, _default, required in parameters
                       if name not in values and required]
            if missing:
                raise RuntimeError(
                    f"Missing battle animation macro arguments: {parts[0]} {missing}")
            arguments = [
                values.get("unused_anim_battler", "ANIM_ATTACKER"),
                values.get("unused_subpriority_offset", "2"),
                values["velocity"], values["shake_timer"],
                values["shake_duration"], values["type"],
            ]
            if "battler_selector" in values:
                arguments.append(values["battler_selector"])
            return ["createsprite gShakeMonOrPlatformSpriteTemplate, " +
                    ", ".join(arguments)]
        expanded: list[str] = []
        for body_line in body:
            # The selected source macros are pure wrappers.  Do not silently
            # flatten conditional assembler logic; if a future selected move
            # needs it, the generator must be taught that macro explicitly.
            if any(body_line.strip().startswith(prefix)
                   for prefix in (".if", ".else", ".endif", ".warning", ".error")):
                # This macro is outside the selected source path. Preserve
                # its invocation so a future move that actually reaches it
                # fails the source audit explicitly instead of being silently
                # approximated.
                return [line]
            substituted = re.sub(
                r"\\([A-Za-z_][A-Za-z0-9_]*)",
                lambda token: values.get(token.group(1), token.group(0)),
                body_line)
            expanded.extend(expand_line(substituted, depth + 1))
        return expanded

    output: list[str] = []
    for line in script.splitlines():
        output.extend(expand_line(line, 0))
    return "\n".join(output) + "\n"


FIRERED_SCRIPT = (D / "data/battle_anim_scripts.s").read_text(encoding="utf-8")
EXPANSION_SCRIPT = _expand_battle_script_macros(
    (EXPANSION / "data/battle_anim_scripts.s").read_text(encoding="utf-8"),
    EXPANSION / "asm/macros/battle_anim_script.inc")
# Keep the original catalog intact and append the complete Gen-IV and Gen-V
# program ranges from pokeemerald-expansion. Their ids are above FireRed's
# 354, so the merged arrays remain sorted and the existing VM needs no
# numeric remapping.
SCRIPT = FIRERED_SCRIPT + "\n" + EXPANSION_SCRIPT
MOVES = (D / "include/constants/moves.h").read_text(encoding="utf-8")
GRAPHICS = (D / "src/graphics.c").read_text(encoding="utf-8")
BATTLE_DATA = (D / "src/data/battle_anim.h").read_text(encoding="utf-8")
BATTLE_ANIM_CONSTANTS = (
    D / "include/constants/battle_anim.h").read_text(encoding="utf-8")
GRAPHICS_RULES = (D / "graphics_file_rules.mk").read_text(encoding="utf-8")
SPRITES = D / "graphics/battle_anims/sprites"
EXPANSION_GRAPHICS = (EXPANSION / "src/graphics.c").read_text(encoding="utf-8")
EXPANSION_BATTLE_DATA = (EXPANSION / "src/data/battle_anim.h").read_text(encoding="utf-8")
EXPANSION_GRAPHICS_RULES = (EXPANSION / "graphics_file_rules.mk").read_text(encoding="utf-8")
EXPANSION_SPRITES = EXPANSION / "graphics/battle_anims/sprites"
# Expansion supplies the exact authored scripts for every post-FireRed move.
# Keep the catalog continuous through Pokemon X/Y's Light of Ruin (#617), so
# a newly generated Kalos move can never silently fall back to no animation.
EXPANSION_MOVE_IDS = set(range(355, 618))
SOURCE_NUMERIC_CONSTANTS: dict[str, int] = {}


def _parse_source_int(text: str) -> int:
    """Evaluate the numeric arithmetic used by GBA ANIMCMD declarations."""
    node = ast.parse(text.strip(), mode="eval").body

    def visit(value: ast.AST) -> int:
        if isinstance(value, ast.Constant) and isinstance(value.value, int):
            return int(value.value)
        if isinstance(value, ast.Name) and value.id in SOURCE_NUMERIC_CONSTANTS:
            return SOURCE_NUMERIC_CONSTANTS[value.id]
        if isinstance(value, ast.UnaryOp):
            operand = visit(value.operand)
            if isinstance(value.op, ast.USub):
                return -operand
            if isinstance(value.op, ast.UAdd):
                return operand
        if isinstance(value, ast.BinOp):
            left, right = visit(value.left), visit(value.right)
            if isinstance(value.op, ast.Add): return left + right
            if isinstance(value.op, ast.Sub): return left - right
            if isinstance(value.op, ast.Mult): return left * right
            if isinstance(value.op, (ast.Div, ast.FloorDiv)): return left // right
        raise ValueError(f"Unsupported animation constant: {text}")

    return visit(node)

# Battle animation scripts are deliberately split into hundreds of reusable
# `call LABEL` fragments.  Looking only between `Move_FOO:` and its `end`
# misses the graphics and visual tasks owned by those fragments (Surf, Razor
# Wind, Dig, etc.) and previously replaced them with an unrelated HIT sprite.
# Index every label once and expand the reachable call graph for each move.
# The original assembly contains a few labels with whitespace before ':'
# (HydroPumpHitSplats is one). Accept assembler-valid spacing so those calls
# cannot disappear from the generated program or audit.
_label_matches = list(re.finditer(
    r"^[ \t]*([A-Za-z0-9_]+)[ \t]*:[^\r\n]*$", SCRIPT, re.M))
LABEL_BODIES: dict[str, str] = {}
LABEL_NEXT: dict[str, str] = {}
for index, match in enumerate(_label_matches):
    end = _label_matches[index + 1].start() if index + 1 < len(_label_matches) else len(SCRIPT)
    LABEL_BODIES[match.group(1)] = SCRIPT[match.end():end]
    if index + 1 < len(_label_matches):
        LABEL_NEXT[match.group(1)] = _label_matches[index + 1].group(1)


def has_physical_fallthrough(label: str) -> bool:
    """Whether execution reaches the next assembly label without a jump/end.

    FireRed deliberately places shared continuations immediately after a move
    label.  A label is not a function boundary in assembly: BLIZZARD, for
    example, conditionally chooses its background and then falls straight
    into BlizzardContinue. Treating every label as an implicit return erased
    all of its ice-crystal sprites from the generated bytecode.
    """
    body = LABEL_BODIES.get(label, "")
    for source_line in body.splitlines():
        line = source_line.split("@", 1)[0].strip()
        if not line:
            continue
        command = line.split(None, 1)[0].lower()
        if command in {"goto", "choosetwoturnanim", "return", "end"}:
            return False
    return label in LABEL_NEXT


def expanded_script(label: str) -> tuple[str, bool]:
    """Return a move and all recursively called animation fragments once."""
    if label not in LABEL_BODIES:
        return "", False
    ordered: list[str] = []
    visited: set[str] = set()

    def visit(current: str) -> None:
        if current in visited or current not in LABEL_BODIES:
            return
        visited.add(current)
        body = LABEL_BODIES[current]
        ordered.append(body)
        control_commands = {
            "call", "goto", "jumpargeq", "jumpifmoveturn", "jumpifcontest",
            "jumprettrue", "jumpretfalse", "jumpreteq", "choosetwoturnanim",
        }
        for line in body.splitlines():
            tokens = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", line)
            if not tokens or tokens[0].lower() not in control_commands:
                continue
            for target in tokens[1:]:
                if target in LABEL_BODIES:
                    visit(target)
        if has_physical_fallthrough(current):
            visit(LABEL_NEXT[current])

    visit(label)
    return "\n".join(ordered), True


def reachable_animation_labels(label: str) -> list[str]:
    """Return the complete FireRed control-flow graph in stable DFS order."""
    ordered: list[str] = []
    visited: set[str] = set()

    def visit(current: str) -> None:
        if current in visited or current not in LABEL_BODIES:
            return
        visited.add(current)
        ordered.append(current)
        for source_line in LABEL_BODIES[current].splitlines():
            line = source_line.split("@", 1)[0].strip()
            if not line:
                continue
            parts = line.split(None, 1)
            command = parts[0].lower()
            if command not in {
                    "call", "goto", "jumpargeq", "jumpifmoveturn",
                    "jumpifcontest", "jumprettrue", "jumpretfalse",
                    "jumpreteq", "choosetwoturnanim"}:
                continue
            operands = split_operands(parts[1]) if len(parts) > 1 else []
            if command == "choosetwoturnanim":
                operands = [token for token in re.findall(
                    r"[A-Za-z_][A-Za-z0-9_]*", parts[1] if len(parts) > 1 else "")
                    if token in LABEL_BODIES]
            for operand in operands:
                if operand in LABEL_BODIES:
                    visit(operand)
        if has_physical_fallthrough(current):
            visit(LABEL_NEXT[current])

    visit(label)
    return ordered


def expanded_calls_with_multiplicity(label: str, stack: tuple[str, ...] = ()) -> str:
    """Expand each `call` occurrence for particle/cadence accounting.

    A helper such as PsybeamRings is intentionally called eleven times. The
    unique call graph is right for asset discovery, but counting that helper
    once collapses eleven overlapping sprites into a single particle.
    """
    if label not in LABEL_BODIES or label in stack:
        return ""
    body = LABEL_BODIES[label]
    chunks = [body]
    next_stack = stack + (label,)
    for target in re.findall(r"^\s*call\s+([A-Za-z0-9_]+)\s*$", body, re.M):
        chunks.append(expanded_calls_with_multiplicity(target, next_stack))
    if has_physical_fallthrough(label):
        chunks.append(expanded_calls_with_multiplicity(LABEL_NEXT[label], next_stack))
    return "\n".join(chunks)


def snake(value: str) -> str:
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value).lower()
    return re.sub(r"[^a-z0-9]+", "_", value).strip("_")


def symbol_paths(kind: str, source_text: str, source_name: str,
                 extension: str) -> dict[str, tuple[str, str]]:
    """Map a GBA graphics symbol to its source tree and stable asset stem."""
    paths: dict[str, tuple[str, str]] = {}
    legacy = re.compile(
        rf'const (?:u32|u16)\s+(gBattleAnimSprite{kind}_\w+)\[\]\s*=\s*'
        rf'INCBIN_U(?:32|16)\("graphics/battle_anims/sprites/([^\"]+)\.'
        rf'{extension}\.lz"\)')
    modern = re.compile(
        rf'const (?:u32|u16)\s+(gBattleAnimSprite{kind}_\w+)\[\]\s*=\s*'
        rf'INCGFX_U(?:32|16)\("graphics/battle_anims/sprites/([^\"]+)\.'
        rf'(?:png|pal|4bpp)"\s*,\s*"(?:\.{extension}(?:\.smol)?|\.smol)"\)')
    for match in legacy.finditer(source_text):
        paths[match.group(1)] = (source_name, match.group(2))
    for match in modern.finditer(source_text):
        paths[match.group(1)] = (source_name, match.group(2))
    return paths


gfx_symbol_paths_by_source = {
    "firered": symbol_paths("Gfx", GRAPHICS, "firered", "4bpp"),
    "expansion": symbol_paths("Gfx", EXPANSION_GRAPHICS, "expansion", "4bpp"),
}
palette_symbol_paths_by_source = {
    "firered": symbol_paths("Pal", GRAPHICS, "firered", "gbapal"),
    "expansion": symbol_paths("Pal", EXPANSION_GRAPHICS, "expansion", "gbapal"),
}
# Keep the legacy names available to diagnostics, with FireRed taking
# precedence when both trees declare the same C symbol.
gfx_symbol_paths = dict(gfx_symbol_paths_by_source["firered"])
gfx_symbol_paths.update({key: value for key, value in
                         gfx_symbol_paths_by_source["expansion"].items()
                         if key not in gfx_symbol_paths})
palette_symbol_paths = dict(palette_symbol_paths_by_source["firered"])
palette_symbol_paths.update({key: value for key, value in
                             palette_symbol_paths_by_source["expansion"].items()
                             if key not in palette_symbol_paths})


def animation_source_dir(source_name: str) -> Path:
    return SPRITES if source_name == "firered" else EXPANSION_SPRITES


tag_to_gfx: dict[str, tuple[str, str]] = {}
tag_to_palette: dict[str, tuple[str, str]] = {}
tag_to_gfx_by_source: dict[str, dict[str, tuple[str, str]]] = {
    "firered": {}, "expansion": {},
}
tag_to_palette_by_source: dict[str, dict[str, tuple[str, str]]] = {
    "firered": {}, "expansion": {},
}


def load_animation_table(source_text: str, source_name: str,
                         table_text: str, table_kind: str) -> None:
    source_gfx_paths = gfx_symbol_paths_by_source[source_name]
    source_palette_paths = palette_symbol_paths_by_source[source_name]
    # FireRed uses a brace table while the expansion uses BATTLE_ANIMATION.
    if table_kind == "firered":
        entries = re.finditer(
            r"\{\s*(gBattleAnimSpriteGfx_\w+)\s*,\s*0x[0-9a-fA-F]+\s*,\s*"
            r"ANIM_TAG_([A-Z0-9_]+)\s*\}", table_text)
        palette_entries = re.finditer(
            r"\{\s*(gBattleAnimSpritePal_\w+)\s*,\s*ANIM_TAG_([A-Z0-9_]+)\s*\}",
            table_text)
    else:
        # BATTLE_ANIMATION also contains build-flag ternaries such as
        # `new_particle ? g...New : g...`.  Select the final (legacy/default)
        # branch while retaining the expansion's own source asset.  The old
        # regex silently dropped those tags, producing purple/missing cels.
        expansion_entries: list[tuple[object, str, str]] = []
        expansion_palette_entries: list[tuple[object, str, str]] = []
        for line in table_text.splitlines():
            match = re.search(
                r"BATTLE_ANIMATION\(\s*ANIM_TAG_([A-Z0-9_]+)\s*,\s*(.*?)\)\s*,?\s*$",
                line)
            if not match:
                continue
            tag = match.group(1)
            body = match.group(2)
            gfx_symbols = re.findall(r"gBattleAnimSpriteGfx_\w+", body)
            palette_symbols = re.findall(r"gBattleAnimSpritePal_\w+", body)
            if gfx_symbols:
                expansion_entries.append((match, tag, gfx_symbols[-1]))
            if palette_symbols:
                expansion_palette_entries.append((match, palette_symbols[-1], tag))
        entries = expansion_entries
        palette_entries = expansion_palette_entries
    for entry in entries:
        if table_kind == "firered":
            gfx_symbol, tag = entry.group(1), entry.group(2)
        else:
            _, tag, gfx_symbol = entry
        if gfx_symbol not in source_gfx_paths:
            if source_name == "firered":
                raise RuntimeError(
                    f"Missing {source_name} graphics path for {gfx_symbol}")
            continue
        mapping = source_gfx_paths[gfx_symbol]
        tag_to_gfx_by_source[source_name][tag] = mapping
        # FireRed remains the default for the legacy catalog. Expansion-only
        # tags are added globally so move-level loadspritegfx still resolve.
        if source_name == "firered" or tag not in tag_to_gfx:
            tag_to_gfx[tag] = mapping
    for entry in palette_entries:
        if table_kind == "firered":
            palette_symbol, tag = entry.group(1), entry.group(2)
        else:
            _, palette_symbol, tag = entry
        if palette_symbol not in source_palette_paths:
            if source_name == "firered":
                raise RuntimeError(
                    f"Missing {source_name} palette path for {palette_symbol}")
            continue
        mapping = source_palette_paths[palette_symbol]
        tag_to_palette_by_source[source_name][tag] = mapping
        if source_name == "firered" or tag not in tag_to_palette:
            tag_to_palette[tag] = mapping


load_animation_table(GRAPHICS, "firered", BATTLE_DATA, "firered")
load_animation_table(EXPANSION_GRAPHICS, "expansion",
                     EXPANSION_BATTLE_DATA, "expansion")

# Smokescreen owns a private sheet/palette outside the global ANIM_TAG tables.
# Treat that original private tag as a regular exact asset so its four impact
# quadrants are not emitted as an invisible, tagless callback.
tag_to_gfx["SMOKESCREEN_IMPACT"] = ("firered", "smokescreen_impact")
tag_to_palette["SMOKESCREEN_IMPACT"] = ("firered", "smokescreen_impact")
tag_to_gfx_by_source["firered"]["SMOKESCREEN_IMPACT"] = tag_to_gfx["SMOKESCREEN_IMPACT"]
tag_to_palette_by_source["firered"]["SMOKESCREEN_IMPACT"] = tag_to_palette["SMOKESCREEN_IMPACT"]
PRIVATE_SYNTHETIC_TAGS = {"SMOKESCREEN_IMPACT"}

# A few original sheets are produced by concatenating smaller PNG sources.
# Parse the official make rules rather than guessing which numbered PNG is a
# representative frame (the previous code picked e.g. ice_crystals_4).
composite_sources: dict[str, list[str]] = {}
composite_sources_by_source: dict[str, dict[str, list[str]]] = {
    "firered": {}, "expansion": {},
}
for match in re.finditer(
        r"\$\(BTLANMSPRGFXDIR\)/([a-z0-9_]+)\.4bpp:\s*(.*?)\n\s*@cat\s+\$\^",
        GRAPHICS_RULES, re.S):
    components = re.findall(
        r"\$\(BTLANMSPRGFXDIR\)/([a-z0-9_]+)\.4bpp", match.group(2))
    if components:
        composite_sources[match.group(1)] = components
        composite_sources_by_source["firered"][match.group(1)] = components
for match in re.finditer(
        r"\$\(BTLANMSPRGFXDIR\)/([a-z0-9_]+)\.4bpp:\s*(.*?)\n\s*@cat\s+\$\^",
        EXPANSION_GRAPHICS_RULES, re.S):
    components = re.findall(
        r"\$\(BTLANMSPRGFXDIR\)/([a-z0-9_]+)\.4bpp", match.group(2))
    if components:
        composite_sources_by_source["expansion"][match.group(1)] = components
        composite_sources.setdefault(match.group(1), components)

# SpriteTemplate is the authoritative size of one OAM cel. Multiple templates
# can share a sheet; the largest cel is the safe canonical view because it
# contains every tile used by its smaller variants without ever exposing the
# complete atlas.
tag_oam_sizes: dict[str, set[tuple[int, int]]] = {}
template_pattern = re.compile(
    r"(?:const|static const)\s+struct\s+SpriteTemplate\s+\w+\s*=\s*\{(.*?)\};",
    re.S)
for source_path in (D / "src").glob("battle_anim*.c"):
    source = source_path.read_text(encoding="utf-8", errors="ignore")
    for body in template_pattern.findall(source):
        tag = re.search(r"\.tileTag\s*=\s*ANIM_TAG_([A-Z0-9_]+)", body)
        size = re.search(r"gOamData_[A-Za-z0-9_]+_(\d+)x(\d+)", body)
        if tag and size:
            tag_oam_sizes.setdefault(tag.group(1), set()).add(
                (int(size.group(1)), int(size.group(2))))
for source_path in (EXPANSION / "src").glob("battle_anim*.c"):
    source = source_path.read_text(encoding="utf-8", errors="ignore")
    for body in template_pattern.findall(source):
        tag = re.search(r"\.tileTag\s*=\s*ANIM_TAG_([A-Z0-9_]+)", body)
        size = re.search(r"gOamData_[A-Za-z0-9_]+_(\d+)x(\d+)", body)
        if tag and size:
            tag_oam_sizes.setdefault(tag.group(1), set()).add(
                (int(size.group(1)), int(size.group(2))))
tag_oam_sizes["SMOKESCREEN_IMPACT"] = {(16, 16)}


def visual_asset_spec(tag: str, palette_tag: str | None = None,
                      source_name: str | None = None,
                      palette_source_name: str | None = None) -> dict[str, object]:
    gfx_table = (tag_to_gfx_by_source.get(source_name, tag_to_gfx)
                 if source_name else tag_to_gfx)
    palette_table = (tag_to_palette_by_source.get(
        palette_source_name or source_name, tag_to_palette)
                     if palette_source_name or source_name else tag_to_palette)
    gfx = gfx_table.get(tag)
    palette = palette_table.get(palette_tag or tag)
    if not gfx or not palette:
        raise RuntimeError(
            f"Animation tag has no complete gfx/palette mapping: {tag} "
            f"source={source_name or 'merged'} palette={palette_tag or tag}")
    gfx_source, gfx_name = gfx
    palette_source, palette_name = palette
    sources = composite_sources.get(gfx_name, [gfx_name])
    images: list[tuple[str, int, int]] = []
    for source in sources:
        path = animation_source_dir(gfx_source) / f"{source}.png"
        if not path.exists():
            raise RuntimeError(f"Animation source is missing: {path}")
        with Image.open(path) as image:
            images.append((source, image.width, image.height))

    candidates = [size for size in tag_oam_sizes.get(tag, set())
                  if any(size[0] <= width and size[1] <= height
                         for _, width, height in images)]
    if candidates:
        cell_width, cell_height = max(candidates, key=lambda value: value[0] * value[1])
    else:
        # Tags instantiated exclusively by a visual task have no visible
        # SpriteTemplate in the script. Preserve the old square-strip rule as
        # a bounded fallback, but report it in the audit.
        _, width, height = images[0]
        cell_width = width
        cell_height = width if height >= width and height % width == 0 else height

    frame_boxes: list[dict[str, int | str]] = []
    for source, width, height in images:
        with Image.open(animation_source_dir(gfx_source) / f"{source}.png") as image:
            columns = width // cell_width
            rows = height // cell_height
            if not columns or not rows:
                boxes = [(0, 0, width, height)]
            else:
                boxes = [(column * cell_width, row * cell_height,
                          column * cell_width + cell_width,
                          row * cell_height + cell_height)
                         for row in range(rows) for column in range(columns)]
            for left, top, right, bottom in boxes:
                crop = image.crop((left, top, right, bottom))
                if image.mode == "P":
                    # Pillow 11 exposes the flattened pixels through
                    # getdata(); the newer get_flattened_data() helper is
                    # not present in the bundled development runtime used
                    # to regenerate the catalog.
                    visible = any(value != 0 for value in crop.getdata())
                else:
                    visible = crop.convert("RGBA").getchannel("A").getbbox() is not None
                if visible:
                    frame_boxes.append({"source": source, "x": left, "y": top,
                                        "width": right - left, "height": bottom - top})
                if len(frame_boxes) >= 32:
                    break
        if len(frame_boxes) >= 32:
            break
    if not frame_boxes:
        source, width, height = images[0]
        frame_boxes.append({"source": source, "x": 0, "y": 0,
                            "width": width, "height": height})
    frame_count = len(frame_boxes)
    return {
        "key": snake(tag),
        "tag": tag,
        "gfx": gfx_name,
        "palette": palette_name,
        "source": gfx_source,
        "paletteSource": palette_source,
        "sources": [source for source, _, _ in images],
        "cellWidth": cell_width,
        "cellHeight": cell_height,
        "frameCount": frame_count,
        "frames": frame_boxes,
        "oamSizes": [list(size) for size in sorted(tag_oam_sizes.get(tag, set()))],
        "oamFallback": not bool(candidates),
    }


# ICE_CRYSTALS is a concatenated tile sheet, not a five-frame animation.  Its
# different SpriteTemplates select different tile offsets from that sheet:
# Ice Beam's inner/outer crystals, Blizzard's snowball/large crystal and the
# impact cels are distinct artwork. Export those cels under stable keys rather
# than cycling the entire tag as if every template shared the same frames.
ICE_TEMPLATE_CELS: dict[str, tuple[str, str, int, int, int, int]] = {
    # template: (asset key, source png, x, y, width, height)
    "gIceBeamInnerCrystalSpriteTemplate":
        ("ice_beam_inner_crystal", "ice_crystals_1", 0, 0, 8, 16),
    "gIceBeamOuterCrystalSpriteTemplate":
        ("ice_beam_outer_crystal", "ice_crystals_2", 0, 0, 8, 8),
    "gIceCrystalHitLargeSpriteTemplate":
        ("ice_crystal_hit_large", "ice_crystals_1", 0, 0, 8, 16),
    "gIceCrystalHitSmallSpriteTemplate":
        ("ice_crystal_hit_small", "ice_crystals_2", 0, 0, 8, 8),
    "gSwirlingSnowballSpriteTemplate":
        ("blizzard_snowball", "ice_crystals_2", 0, 8, 8, 8),
    "gBlizzardIceCrystalSpriteTemplate":
        ("blizzard_ice_crystal", "ice_crystals_3", 0, 0, 16, 16),
}

# Several FireRed callbacks select a cel by changing OAM tileNum directly
# instead of calling StartSpriteAnim.  That mutation is outside the AnimCmd
# table and was therefore invisible to the old asset extractor: the runtime
# got only tile zero and displayed the wrong half/variant (or transparent
# tiles).  Materialise those choices as ordinary retained sequences so the
# firmware never needs to reinterpret a raw GBA tile sheet.
MANUAL_TILE_BASE_BIAS: dict[str, int] = {
    "AnimTearDrop": 4,
    "AnimLockOnMoveTarget": 16,
    "AnimClappingHand": 16,
    "AnimSmellingSaltsHand": 16,
    "AnimMudSportDirt": 1,
    "AnimSmallDriftingBubbles": 8,
    # InitIceBallParticle adds eight to the live OAM tile before the sprite
    # animation engine runs. BeginAnim immediately restores the template's
    # sAnim_IceCrystalSmall frame (tile 6); treating the transient write as a
    # retained cel would address tile 14 past FireRed's 14-tile sheet. Keep
    # the actual visible cel while still recording that this write was
    # reviewed deliberately.
    "InitIceBallParticle": 0,
}

# Template-level corrections for callbacks reused with a different compact
# sheet. Thousand Arrows borrows AnimMudSportDirt only for its falling motion;
# its NEEDLE asset is exactly one 16x16 cel, so the dirt callback's +1 tile
# write would address beyond the loaded four-tile resource on the GBA too.
TEMPLATE_MANUAL_TILE_BASE_BIAS: dict[str, int] = {
    "gThousandArrowsGreenArrowTemplate": 0,
}

# callback: (argument index, tile stride, variant count, per-variant h/v flip)
MANUAL_TILE_ARGUMENT_VARIANTS: dict[
        str, tuple[int, int, int, dict[int, tuple[bool, bool]]]] = {
    "AnimDigDirtMound": (1, 8, 2, {}),
    "AnimJaggedMusicNote": (3, 16, 2, {}),
    "AnimZapCannonSpark": (6, 4, 3, {}),
    "AnimSparkElectricityFlashing": (6, 4, 3, {}),
    "AnimElectricity": (3, 4, 3, {1: (True, False), 2: (False, True)}),
    "AnimBrickBreakWallShard": (1, 16, 4, {}),
    "AnimSuperpowerRock": (2, 4, 5, {}),
    "AnimEruptionFallingRock": (4, 16, 4, {}),
}

# These callbacks change tileNum later in their state machine.  Sequence zero
# is the spawn cel; the exact motion profile chooses the later sequence at the
# same tick as the original callback.
MANUAL_TILE_PHASE_VARIANTS: dict[str, tuple[int, ...]] = {
    "AnimThunderWave": (0, 8),
    "AnimSoftBoiledEgg": (0, 16, 32),
    "AnimMiniTwinklingStar": (4, 5),
    "AnimFlatterConfetti": tuple(range(12)),
    "AnimHelpingHandClap": (0, 16),
    "AnimCurseNail": (0, 8, 16, 24),
}

# Some expansion templates intentionally reuse a callback's shared animation
# table while loading a compact one-cel sheet. Keep only the sequence selected
# by the authored script instead of exporting unrelated frames past that
# template's source image.
TEMPLATE_SEQUENCE_LIMITS: dict[str, int] = {
    "gPalmSpriteTemplate": 1,
    # Quick Guard loads a dedicated one-cel 32x32 hand but deliberately
    # reuses the generic hands/feet AnimCmd table. Its authored calls pass
    # animation selector zero, so later fist/foot sequences belong to the
    # shared table rather than this compact sheet.
    "gQuickGuardArmImpactTemplate": 1,
    # Quash likewise loads one dedicated 32x32 palm while borrowing the
    # shared hands/feet table; AnimStompFoot never selects another sequence.
    "gQuashArmHitTemplate": 1,
    # Heart Stamp's dedicated sheet contains exactly two 32x32 hearts and its
    # script selects sequence one. The remaining hands/feet sequences are not
    # part of this asset.
    "gHeartStampSpinningHeartTemplate": 2,
    # Power Orbs reuses the two-frame spore table, but the expansion asset is
    # one 16x16 cel and the authored scripts always select sequence 0.
    "gPowerOrbs_Float": 1,
    # Crush Grip reuses the six rock-size sequences but its purple-hand sheet
    # contains only the default four-tile cel; AnimRockBlastRock never changes
    # the sprite animation index.
    "gCrushGripHandTemplate": 1,
}


def template_visual_asset_spec(template: str, tag: str,
                               source_name: str = "firered") -> dict[str, object] | None:
    override = ICE_TEMPLATE_CELS.get(template)
    if override is not None:
        key, source, x, y, width, height = override
        if tag != "ICE_CRYSTALS":
            raise RuntimeError(f"Ice template {template} unexpectedly uses {tag}")
        path = SPRITES / f"{source}.png"
        if not path.exists():
            raise RuntimeError(f"Missing exact FireRed template source: {path}")
        with Image.open(path) as image:
            if x < 0 or y < 0 or x + width > image.width or y + height > image.height:
                raise RuntimeError(
                    f"Invalid exact cel for {template}: {(x, y, width, height)} in {image.size}")
        return {
            "key": key,
            "tag": tag,
            "gfx": "ice_crystals",
            "palette": "ice_crystals",
            "sources": [source],
            "cellWidth": width,
            "cellHeight": height,
            "frameCount": 1,
            "frames": [{"source": source, "x": x, "y": y,
                        "width": width, "height": height}],
            "oamSizes": [[width, height]],
            "oamFallback": False,
            "template": template,
            "sequences": [{
                "steps": [{"frame": 0, "duration": 1}],
                "loopStep": 0xFF,
            }],
        }

    spec = template_specs.get(template)
    gfx = tag_to_gfx_by_source.get(source_name, tag_to_gfx).get(tag)
    palette_tag = str(spec.get("paletteTag", tag)) if spec else tag
    palette = tag_to_palette_by_source.get(source_name, tag_to_palette).get(palette_tag)
    if not spec or not gfx or not palette:
        return None
    gfx_source, gfx_name = gfx
    palette_source, palette_name = palette
    width = int(spec.get("width", 0))
    height = int(spec.get("height", 0))
    if width <= 0 or height <= 0 or width % 8 or height % 8:
        raise RuntimeError(f"Invalid OAM dimensions for {template}: {width}x{height}")

    sources = composite_sources_by_source.get(source_name, {}).get(
        gfx_name, composite_sources.get(gfx_name, [gfx_name]))
    tiles: list[dict[str, int | str]] = []
    for source in sources:
        path = animation_source_dir(gfx_source) / f"{source}.png"
        if not path.exists():
            raise RuntimeError(f"Missing animation template sheet: {path}")
        with Image.open(path) as image:
            if image.width % 8 or image.height % 8:
                raise RuntimeError(f"Non-tile animation source: {path} {image.size}")
            for tile_y in range(0, image.height, 8):
                for tile_x in range(0, image.width, 8):
                    tiles.append({"source": source, "x": tile_x, "y": tile_y})

    animation_table = str(spec.get("animations", ""))
    command_arrays = anim_pointer_tables.get(animation_table, [])
    command_sequences = [anim_command_arrays[command_array]
                         for command_array in command_arrays
                         if command_array in anim_command_arrays]
    if not command_sequences:
        command_sequences = [{
            "steps": [{"tile": 0, "duration": 1,
                       "hFlip": False, "vFlip": False}],
            "loopStep": 0xFF,
        }]

    def shifted_sequence(sequence: dict[str, object], tile_offset: int,
                         h_flip: bool = False,
                         v_flip: bool = False) -> dict[str, object]:
        return {
            "steps": [{
                **step,
                "tile": int(step["tile"]) + tile_offset,
                "hFlip": bool(step["hFlip"]) ^ h_flip,
                "vFlip": bool(step["vFlip"]) ^ v_flip,
            } for step in sequence["steps"]],
            "loopStep": int(sequence.get("loopStep", 0xFF)),
        }

    callback = str(spec.get("callback", ""))
    base_bias = TEMPLATE_MANUAL_TILE_BASE_BIAS.get(
        template, MANUAL_TILE_BASE_BIAS.get(callback, 0))
    if base_bias:
        command_sequences = [shifted_sequence(sequence, base_bias)
                             for sequence in command_sequences]
    argument_variants = MANUAL_TILE_ARGUMENT_VARIANTS.get(callback)
    phase_variants = MANUAL_TILE_PHASE_VARIANTS.get(callback)
    if argument_variants or phase_variants:
        if len(command_sequences) != 1:
            raise RuntimeError(
                f"Manual OAM tile selector must start from one AnimCmd sequence: "
                f"{template}/{callback} has {len(command_sequences)}")
        base_sequence = command_sequences[0]
        if argument_variants:
            _, stride, count, flips = argument_variants
            command_sequences = [shifted_sequence(
                base_sequence, variant * stride,
                flips.get(variant, (False, False))[0],
                flips.get(variant, (False, False))[1])
                for variant in range(count)]
        else:
            command_sequences = [shifted_sequence(base_sequence, offset)
                                 for offset in phase_variants or (0,)]
    sequence_limit = TEMPLATE_SEQUENCE_LIMITS.get(template)
    if sequence_limit is not None:
        command_sequences = command_sequences[:sequence_limit]
    frame_commands = [command for sequence in command_sequences
                      for command in sequence["steps"]]

    # Pointer tables often expose several orientation variants. Keep every
    # distinct cel, but never cycle unrelated tiles outside this template.
    distinct_commands: list[dict[str, object]] = []
    seen_commands: set[tuple[int, bool, bool]] = set()
    for command in frame_commands:
        identity = (int(command["tile"]), bool(command["hFlip"]),
                    bool(command["vFlip"]))
        if identity in seen_commands:
            continue
        seen_commands.add(identity)
        distinct_commands.append(command)
        # A single FireRed animation variant never owns more than eight cels,
        # but a pointer table may contain several mutually exclusive
        # orientations. Export all of them and let the firmware preload only
        # the selected sequence. Capping the flattened table here used to
        # silently delete valid right/left or attacker/target variants.

    frame_specs: list[dict[str, object]] = []
    tiles_wide, tiles_high = width // 8, height // 8
    for command in distinct_commands:
        first_tile = int(command["tile"])
        frame_tiles: list[dict[str, int | str]] = []
        for tile_y in range(tiles_high):
            for tile_x in range(tiles_wide):
                tile_index = first_tile + tile_y * tiles_wide + tile_x
                if tile_index < 0 or tile_index >= len(tiles):
                    raise RuntimeError(
                        f"Template {template} cel exceeds {gfx}: tile={tile_index}, "
                        f"available={len(tiles)}, size={width}x{height}")
                source_tile = tiles[tile_index]
                frame_tiles.append({**source_tile,
                                    "destX": tile_x * 8, "destY": tile_y * 8})
        frame_specs.append({
            "width": width, "height": height, "tiles": frame_tiles,
            "hFlip": bool(command["hFlip"]),
            "vFlip": bool(command["vFlip"]),
            "duration": int(command["duration"]),
        })

    command_to_frame: dict[tuple[int, bool, bool], int] = {}
    for frame_index, command in enumerate(distinct_commands):
        command_to_frame[(int(command["tile"]), bool(command["hFlip"]),
                          bool(command["vFlip"]))] = frame_index
    sequences: list[dict[str, object]] = []
    for sequence in command_sequences:
        steps = [{
            "frame": command_to_frame[(int(command["tile"]),
                                       bool(command["hFlip"]),
                                       bool(command["vFlip"]))],
            "duration": max(1, int(command["duration"])),
        } for command in sequence["steps"]]
        sequences.append({
            "steps": steps or [{"frame": 0, "duration": 1}],
            "loopStep": int(sequence.get("loopStep", 0xFF)),
        })
    for sequence_index, sequence in enumerate(sequences):
        resident_frames = {int(step["frame"]) for step in sequence["steps"]}
        if len(resident_frames) > 8:
            raise RuntimeError(
                f"FireRed cel sequence exceeds the CYD resident cache contract: "
                f"{template}[{sequence_index}]={len(resident_frames)}")

    return {
        "key": f"tpl_{hashlib.sha1(template.encode('ascii')).hexdigest()[:12]}",
        "tag": tag,
        "gfx": gfx_name,
        "palette": palette_name,
        "source": gfx_source,
        "paletteSource": palette_source,
        "sources": sources,
        "cellWidth": width,
        "cellHeight": height,
        "frameCount": len(frame_specs),
        "frames": frame_specs,
        "oamSizes": [[width, height]],
        "oamFallback": False,
        "template": template,
        "animations": animation_table,
        "sequences": sequences,
    }


def split_operands(text: str) -> list[str]:
    """Split an assembler operand list without splitting RGB(...) calls."""
    result: list[str] = []
    start = 0
    depth = 0
    for index, character in enumerate(text):
        if character == "(":
            depth += 1
        elif character == ")":
            depth = max(0, depth - 1)
        elif character == "," and depth == 0:
            result.append(text[start:index].strip())
            start = index + 1
    tail = text[start:].strip()
    if tail:
        result.append(tail)
    return result


def _gba_rgb(red: int, green: int, blue: int) -> int:
    """Pack native five-bit GBA channels exactly like rgb.h's RGB macro."""
    return red | (green << 5) | (blue << 10)


def _gba_rgb2gba(red: int, green: int, blue: int) -> int:
    """Quantize eight-bit channels exactly like rgb.h's RGB2GBA macro."""
    return ((red >> 3) & 31) | (((green >> 3) & 31) << 5) | (((blue >> 3) & 31) << 10)


EXPRESSION_CONSTANTS = {
    "FALSE": 0,
    "TRUE": 1,
    "ARG_RET_ID": 7,
    "ANIM_ATTACKER": 0,
    "ANIM_TARGET": 1,
    "ANIM_ATK_PARTNER": 2,
    "ANIM_DEF_PARTNER": 3,
    "SOUND_PAN_ATTACKER": -64,
    "SOUND_PAN_TARGET": 63,
    "SOUND_PAN_CENTER": 0,
    # Keep the source rgb.h constants verbatim.  Palette tasks distinguish
    # these values; silently resolving an expansion-only name to zero turns a
    # white flash (notably Roar of Time) into a second black fade.
    "RGB_ALPHA": 1 << 15,
    "RGB_BLACK": _gba_rgb(0, 0, 0),
    "RGB_WHITE": _gba_rgb(31, 31, 31),
    "RGB_GRAY": _gba_rgb(15, 15, 15),
    "RGB_RED": _gba_rgb(31, 0, 0),
    "RGB_GREEN": _gba_rgb(0, 31, 0),
    "RGB_BLUE": _gba_rgb(0, 0, 31),
    "RGB_PURPLE": _gba_rgb(24, 0, 24),
    "RGB_YELLOW": _gba_rgb(31, 31, 0),
    "RGB_MAGENTA": _gba_rgb(31, 0, 31),
    "RGB_CYAN": _gba_rgb(0, 31, 31),
    "RGB_WHITEALPHA": _gba_rgb(31, 31, 31) | (1 << 15),
    "RGB_LIME_GREEN": _gba_rgb2gba(222, 230, 49),
    "F_PAL_BG": 1,
    "F_PAL_ATTACKER": 2,
    "F_PAL_TARGET": 4,
    "F_PAL_ATK_PARTNER": 8,
    "F_PAL_DEF_PARTNER": 16,
    "F_PAL_ATK_SIDE": 10,
    "F_PAL_DEF_SIDE": 20,
    "F_PAL_BATTLERS": 30,
    "F_PAL_ANIM_1": 32,
    "F_PAL_ANIM_2": 64,
    "BATTLE_TERRAIN_GRASS": 0,
    "BATTLE_TERRAIN_LONG_GRASS": 1,
    "BATTLE_TERRAIN_SAND": 2,
    "BATTLE_TERRAIN_UNDERWATER": 3,
    "BATTLE_TERRAIN_WATER": 4,
    "BATTLE_TERRAIN_POND": 5,
    "BATTLE_TERRAIN_MOUNTAIN": 6,
    "BATTLE_TERRAIN_CAVE": 7,
    "BATTLE_TERRAIN_BUILDING": 8,
    "BATTLE_TERRAIN_PLAIN": 9,
    "ANIM_WEATHER_NONE": 0,
    "ANIM_WEATHER_SUN": 1,
    "ANIM_WEATHER_RAIN": 2,
    "ANIM_WEATHER_SANDSTORM": 3,
    "ANIM_WEATHER_HAIL": 4,
}

# Every named source color must be explicit above.  Other script operands have
# historical zero-default behavior, but a missing RGB symbol is never a valid
# approximation: it changes the authored palette animation into black.
_used_rgb_constants = set(re.findall(r"\bRGB_[A-Z0-9_]+\b", SCRIPT))
_missing_rgb_constants = sorted(_used_rgb_constants - EXPRESSION_CONSTANTS.keys())
if _missing_rgb_constants:
    raise RuntimeError(
        "Battle animation source uses unresolved RGB constants: " +
        ", ".join(_missing_rgb_constants))
# Task arguments use the public ANIM_TAG_* constants, while SpriteTemplates
# retain the symbolic tag.  Previously expression_value() silently converted
# every unknown identifier to zero, so all tag-addressed palette tasks acted
# on the same nonexistent palette.  Parse the source constants before any
# script command is translated and reject unknown identifiers below.
EXPRESSION_CONSTANTS["ANIM_SPRITES_START"] = 10000
for _tag_name, _tag_offset in re.findall(
        r"#define\s+(ANIM_TAG_[A-Z0-9_]+)\s+"
        r"\(ANIM_SPRITES_START\s*\+\s*(\d+)\)",
        BATTLE_ANIM_CONSTANTS):
    EXPRESSION_CONSTANTS[_tag_name] = 10000 + int(_tag_offset)
# Gen-IV scripts use expansion-only tags (for example the horseshoe fist and
# hammer graphics). Their numeric tag ids are still needed by palette tasks;
# import them without replacing any FireRed value with a different source
# table entry.
for _tag_name, _tag_offset in re.findall(
        r"#define\s+(ANIM_TAG_[A-Z0-9_]+)\s+"
        r"\(ANIM_SPRITES_START\s*\+\s*(\d+)\)",
        (EXPANSION / "include/constants/battle_anim.h").read_text(
            encoding="utf-8")):
    EXPRESSION_CONSTANTS.setdefault(_tag_name, 10000 + int(_tag_offset))
for _background_id, _background_name in enumerate((
        "BG_NONE", "BG_DARK", "BG_GHOST", "BG_PSYCHIC", "BG_IMPACT_OPPONENT",
        "BG_IMPACT_PLAYER", "BG_IMPACT_CONTESTS", "BG_DRILL", "BG_DRILL_CONTESTS",
        "BG_HIGHSPEED_OPPONENT", "BG_HIGHSPEED_PLAYER", "BG_THUNDER",
        "BG_GUILLOTINE_OPPONENT", "BG_GUILLOTINE_PLAYER", "BG_GUILLOTINE_CONTESTS",
        "BG_ICE", "BG_COSMIC", "BG_IN_AIR", "BG_SKY", "BG_SKY_CONTESTS",
        "BG_AURORA", "BG_FISSURE", "BG_BUG_OPPONENT", "BG_BUG_PLAYER",
        "BG_SOLAR_BEAM_OPPONENT", "BG_SOLAR_BEAM_PLAYER", "BG_SOLAR_BEAM_CONTESTS")):
    EXPRESSION_CONSTANTS[_background_name] = _background_id
# The expansion's BG table continues far beyond FireRed.  The Pokegochi
# archive gives Aura Sphere one explicit local slot, backed by the expansion's
# own Focus Blast tilemap and Aura Sphere palette.
EXPRESSION_CONSTANTS["BG_AURA_SPHERE"] = 27


def _eval_expression_node(node: ast.AST) -> int:
    if isinstance(node, ast.Constant) and isinstance(node.value, int):
        return int(node.value)
    if isinstance(node, ast.Name):
        if node.id.startswith("RGB_") and node.id not in EXPRESSION_CONSTANTS:
            raise RuntimeError(f"Unresolved battle animation color: {node.id}")
        return EXPRESSION_CONSTANTS.get(node.id, 0)
    if isinstance(node, ast.UnaryOp):
        value = _eval_expression_node(node.operand)
        if isinstance(node.op, ast.USub):
            return -value
        if isinstance(node.op, ast.UAdd):
            return value
        if isinstance(node.op, ast.Invert):
            return ~value
    if isinstance(node, ast.BinOp):
        left = _eval_expression_node(node.left)
        right = _eval_expression_node(node.right)
        if isinstance(node.op, ast.BitOr):
            return left | right
        if isinstance(node.op, ast.BitAnd):
            return left & right
        if isinstance(node.op, ast.LShift):
            return left << right
        if isinstance(node.op, ast.RShift):
            return left >> right
        if isinstance(node.op, ast.Add):
            return left + right
        if isinstance(node.op, ast.Sub):
            return left - right
        if isinstance(node.op, ast.Mult):
            return left * right
    if (isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and
            node.func.id in {"RGB", "RGB2", "_RGB", "RGB2GBA"}):
        values = [_eval_expression_node(argument) for argument in node.args]
        if len(values) == 3:
            if node.func.id == "RGB2GBA":
                return _gba_rgb2gba(values[0], values[1], values[2])
            return _gba_rgb(values[0], values[1], values[2])
    return 0


def expression_value(text: str) -> int:
    cleaned = text.strip()
    try:
        value = _eval_expression_node(ast.parse(cleaned, mode="eval").body)
    except (SyntaxError, ValueError):
        value = EXPRESSION_CONSTANTS.get(cleaned, 0)
    # FireRed animation arguments are signed 16-bit values.
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def animation_source_lines(label: str, stack: tuple[str, ...] = ()) -> list[str]:
    """Expand the normal-battle path of one FireRed animation in order.

    `call` and unconditional `goto` preserve multiplicity. Contest-only and
    return-value branches deliberately stay on their normal fall-through
    path. Two-turn moves select their impact path: Pokegochi presents a move
    animation when its battle journal resolves that move's effect.
    """
    if label not in LABEL_BODIES or label in stack or len(stack) > 24:
        return []
    output: list[str] = []
    next_stack = stack + (label,)
    terminated = False
    for source_line in LABEL_BODIES[label].splitlines():
        line = source_line.split("@", 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 1)
        command = parts[0].lower()
        operands = split_operands(parts[1]) if len(parts) > 1 else []
        if command == "call" and operands:
            output.extend(animation_source_lines(operands[0], next_stack))
            continue
        if command == "goto" and operands:
            output.extend(animation_source_lines(operands[0], next_stack))
            terminated = True
            break
        if command == "choosetwoturnanim" and operands:
            # Expansion accepts the two labels separated by whitespace while
            # FireRed commonly emits commas. Normalize both spellings before
            # choosing the attack path for the hardware battle journal.
            choices = [token for token in re.findall(
                r"[A-Za-z_][A-Za-z0-9_]*", parts[1] if len(parts) > 1 else "")
                if token in LABEL_BODIES]
            if choices:
                output.extend(animation_source_lines(choices[-1], next_stack))
            terminated = True
            break
        if command in {"jumpifcontest", "jumpargeq", "jumpifmoveturn",
                       "jumprettrue", "jumpretfalse", "jumpreteq"}:
            continue
        if command == "return":
            terminated = True
            break
        output.append(line)
        if command == "end":
            terminated = True
            break
    if not terminated and has_physical_fallthrough(label):
        output.extend(animation_source_lines(LABEL_NEXT[label], next_stack))
    return output


animation_sources = [
    (source_path.read_text(encoding="utf-8", errors="ignore"), "firered")
    for source_path in (D / "src").glob("battle_anim*.c")
] + [
    (source_path.read_text(encoding="utf-8", errors="ignore"), "expansion")
    for source_path in (EXPANSION / "src").glob("battle_anim*.c")
]

# Modern expansion callbacks keep affine constants next to their source
# tables. Resolve those exact constants before decoding ANIMCMD/AFFINEANIMCMD;
# a missing symbolic value is an audit error when its template is actually
# referenced, never a reason to invent a transform.
for _ in range(8):
    for _source, _ in animation_sources:
        for _name, _value in re.findall(
                r"^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+([^/\r\n]+)",
                _source, re.M):
            try:
                SOURCE_NUMERIC_CONSTANTS[_name] = _parse_source_int(_value)
            except (SyntaxError, ValueError, ZeroDivisionError):
                pass


def c_function_bodies(source: str) -> dict[str, str]:
    result: dict[str, str] = {}
    declaration = re.compile(
        r"(?:^|\n)(?:static\s+)?(?:void|bool8|u8|s8|u16|s16|u32|s32)\s+"
        r"([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*?\)\s*\{", re.S)
    for match in declaration.finditer(source):
        depth = 1
        cursor = match.end()
        while cursor < len(source) and depth:
            if source[cursor] == "{":
                depth += 1
            elif source[cursor] == "}":
                depth -= 1
            cursor += 1
        if depth == 0:
            result[match.group(1)] = source[match.end():cursor - 1]
    return result


animation_function_bodies: dict[str, str] = {}
for source, source_name in animation_sources:
    functions = c_function_bodies(source)
    if source_name == "firered":
        animation_function_bodies.update(functions)
    else:
        for name, body in functions.items():
            animation_function_bodies.setdefault(name, body)


def expanded_callback_body(callback: str, stack: tuple[str, ...] = ()) -> str:
    if callback not in animation_function_bodies or callback in stack or len(stack) > 5:
        return ""
    body = animation_function_bodies[callback]
    chunks = [body]
    next_stack = stack + (callback,)
    for candidate in set(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", body)):
        if candidate in animation_function_bodies:
            chunks.append(expanded_callback_body(candidate, next_stack))
    return "\n".join(chunks)


def expanded_sprite_motion_body(callback: str,
                                stack: tuple[str, ...] = ()) -> str:
    """Expand the complete callback state machine used to move one sprite.

    Most FireRed SpriteTemplate callbacks only initialise data[] and install a
    second callback (``sprite->callback = Foo_Step``).  Looking exclusively at
    direct calls made those sprites appear static even though Foo_Step moved
    x2/y2 every frame.  This expansion is deliberately used *only* for motion
    classification: origin/travel inference still reads the initial callback,
    where the attacker/target coordinate setup actually lives.
    """
    if callback not in animation_function_bodies or callback in stack or len(stack) > 12:
        return ""
    body = animation_function_bodies[callback]
    chunks = [body]
    next_stack = stack + (callback,)
    # Follow only callbacks stored for a later frame. Direct helper calls are
    # already visible by name in this body; recursively expanding coordinate,
    # sound and destruction helpers pollutes the result with unrelated x2/y2
    # writes from the whole engine.
    candidates = set(re.findall(
        r"(?:->|\.)\s*callback\s*=\s*([A-Za-z_][A-Za-z0-9_]*)", body))
    candidates.update(re.findall(
        r"StoreSpriteCallbackInData\d*\s*\([^,]+,\s*"
        r"([A-Za-z_][A-Za-z0-9_]*)", body))
    # Destroy/wait plumbing never contributes coordinates and following it
    # only drags unrelated engine helpers into the semantic classifier.
    ignored = {
        "DestroyAnimSprite", "DestroySpriteAndMatrix", "DestroyAnimSpriteAfterTimer",
        "RunStoredCallbackWhenAnimEnds", "RunStoredCallbackWhenAffineAnimEnds",
        "WaitAnimForDuration", "SpriteCallbackDummy",
    }
    for candidate in candidates - ignored:
        if candidate in animation_function_bodies:
            chunks.append(expanded_sprite_motion_body(candidate, next_stack))
    return "\n".join(chunks)


def installed_sprite_callbacks(callback: str,
                               stack: tuple[str, ...] = ()) -> list[str]:
    """Return every per-frame callback installed by a sprite state machine."""
    if callback not in animation_function_bodies or callback in stack or len(stack) > 12:
        return []
    body = animation_function_bodies[callback]
    candidates = set(re.findall(
        r"(?:->|\.)\s*callback\s*=\s*([A-Za-z_][A-Za-z0-9_]*)", body))
    candidates.update(re.findall(
        r"StoreSpriteCallbackInData\d*\s*\([^,]+,\s*"
        r"([A-Za-z_][A-Za-z0-9_]*)", body))
    ignored = {
        "DestroyAnimSprite", "DestroySpriteAndMatrix", "DestroyAnimSpriteAfterTimer",
        "RunStoredCallbackWhenAnimEnds", "RunStoredCallbackWhenAffineAnimEnds",
        "WaitAnimForDuration", "SpriteCallbackDummy",
    }
    result: list[str] = []
    next_stack = stack + (callback,)
    for candidate in sorted(candidates - ignored):
        if candidate not in animation_function_bodies:
            continue
        result.append(candidate)
        for nested in installed_sprite_callbacks(candidate, next_stack):
            if nested not in result:
                result.append(nested)
    return result


def installed_sprite_moves_coordinates(callback: str) -> bool:
    """Prove whether an installed state callback changes screen position."""
    for state_callback in installed_sprite_callbacks(callback):
        body = animation_function_bodies[state_callback]
        if re.search(
                r"sprite->(?:x2|y2|x|y)\s*[+\-]?=|"
                r"\b(?:StartAnimLinearTranslation|InitAndRunAnimFastLinearTranslation|"
                r"TranslateSprite(?:Linear|InCircle|InEllipse))\s*\(", body):
            return True
    return False


def expanded_sprite_initializer_body(callback: str,
                                     stack: tuple[str, ...] = ()) -> str:
    """Expand immediate helpers but exclude callbacks installed for later.

    Sprite animation selection happens before the first rendered frame. A
    StartSpriteAnim call in a later state is a phase transition, not the
    initial variant; conflating both used to select the ending cel at spawn.
    """
    if callback not in animation_function_bodies or callback in stack or len(stack) > 6:
        return ""
    body = animation_function_bodies[callback]
    installed = set(re.findall(
        r"(?:->|\.)\s*callback\s*=\s*([A-Za-z_][A-Za-z0-9_]*)", body))
    installed.update(re.findall(
        r"StoreSpriteCallbackInData\d*\s*\([^,]+,\s*"
        r"([A-Za-z_][A-Za-z0-9_]*)", body))
    direct = set(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", body))
    chunks = [body]
    next_stack = stack + (callback,)
    for helper in sorted(direct - installed):
        if helper in animation_function_bodies:
            chunks.append(expanded_sprite_initializer_body(helper, next_stack))
    return "\n".join(chunks)


def expanded_task_callback_body(callback: str,
                                stack: tuple[str, ...] = ()) -> str:
    """Expand a task's installed state callbacks as well as direct calls.

    Task state machines install their next function through `task->func =`
    instead of invoking it.  The real CreateSprite calls are normally in those
    state functions.  Sprite callback analysis intentionally keeps using the
    narrower `expanded_callback_body`: following every installed destructor
    there would pollute its origin/motion inference.
    """
    if callback not in animation_function_bodies or callback in stack or len(stack) > 9:
        return ""
    body = animation_function_bodies[callback]
    chunks = [body]
    next_stack = stack + (callback,)
    candidates = set(re.findall(
        r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", body))
    candidates.update(re.findall(
        r"(?:->|\.)\s*(?:func|callback)\s*=\s*"
        r"([A-Za-z_][A-Za-z0-9_]*)", body))
    candidates.update(re.findall(
        r"StoreSpriteCallbackInData\d*\s*\([^,]+,\s*"
        r"([A-Za-z_][A-Za-z0-9_]*)", body))
    for candidate in candidates:
        if candidate in animation_function_bodies:
            chunks.append(expanded_task_callback_body(candidate, next_stack))
    return "\n".join(chunks)


# Resolve SpriteTemplate -> animation table -> ANIMCMD_FRAME tile offsets. This
# is the missing layer between an ANIM_TAG sheet and the actual cel selected by
# FireRed. Without it every template sharing a tag receives the whole atlas and
# unrelated effects are incorrectly cycled as frames.
anim_command_arrays: dict[str, dict[str, object]] = {}
anim_pointer_tables: dict[str, list[str]] = {}
for source, _source_name in animation_sources:
    for match in re.finditer(
            r"(?:static\s+)?const\s+union\s+AnimCmd\s+(\w+)\[\]\s*=\s*"
            r"\{(.*?)\};", source, re.S):
        raw_commands: list[dict[str, object]] = []
        for command_match in re.finditer(
                r"ANIMCMD_(FRAME|LOOP|JUMP|END)\s*"
                r"(?:\(((?:[^()]|\([^()]*\))*)\))?",
                match.group(2), re.S):
            kind = command_match.group(1)
            operands = split_operands(command_match.group(2) or "")
            if kind == "FRAME" and operands:
                raw_commands.append({
                    "kind": kind,
                    "tile": _parse_source_int(operands[0]),
                    "duration": _parse_source_int(operands[1])
                    if len(operands) > 1 else 1,
                    "hFlip": any("hFlip" in operand and "TRUE" in operand
                                 for operand in operands[2:]),
                    "vFlip": any("vFlip" in operand and "TRUE" in operand
                                 for operand in operands[2:]),
                })
            elif kind in {"LOOP", "JUMP"} and operands:
                raw_commands.append({"kind": kind,
                                     "value": _parse_source_int(operands[0])})
            elif kind == "END":
                raw_commands.append({"kind": kind})

        # Materialise finite ANIMCMD_LOOP blocks exactly. Infinite
        # ANIMCMD_JUMP loops retain the step at which playback wraps, so the
        # ESP32 can reproduce the source timing without storing an unbounded
        # timeline.
        steps: list[dict[str, object]] = []
        command_to_step: dict[int, int] = {}
        loop_start = 0
        infinite_loop_step = 0xFF
        for command_index, command in enumerate(raw_commands):
            command_to_step[command_index] = len(steps)
            kind = command["kind"]
            if kind == "FRAME":
                steps.append({key: value for key, value in command.items()
                              if key != "kind"})
            elif kind == "LOOP":
                count = int(command["value"])
                if count == 0:
                    loop_start = len(steps)
                elif loop_start < len(steps):
                    segment = [dict(step) for step in steps[loop_start:]]
                    for _ in range(count):
                        steps.extend(dict(step) for step in segment)
            elif kind == "JUMP":
                infinite_loop_step = command_to_step.get(int(command["value"]), 0)
                break
            elif kind == "END":
                break
        if len(steps) > 255:
            raise RuntimeError(
                f"Expanded animation sequence is too long: {match.group(1)} ({len(steps)})")
        if _source_name == "expansion" and match.group(1) in anim_command_arrays:
            continue
        anim_command_arrays[match.group(1)] = {
            "steps": steps,
            "loopStep": infinite_loop_step,
        }
    for match in re.finditer(
        r"(?:static\s+)?const\s+union\s+AnimCmd\s*\*\s*const\s+"
            r"(\w+)\[\]\s*=\s*(?://[^\r\n]*)?\s*\{(.*?)\};", source, re.S):
        if _source_name == "expansion" and match.group(1) in anim_pointer_tables:
            continue
        anim_pointer_tables[match.group(1)] = re.findall(
            r"^\s*(\w+)\s*,?\s*$", match.group(2), re.M)

# Affine commands are a second, independent animation stream attached to a
# SpriteTemplate. FireRed expresses scale and rotation as an initial absolute
# frame (duration zero) followed by per-tick deltas. Keeping this stream as
# commands is compact enough for flash and lets firmware evaluate the exact
# transform at its sampled GBA age.
affine_command_arrays: dict[str, dict[str, object]] = {}
affine_pointer_tables: dict[str, list[str]] = {}
for source, _source_name in animation_sources:
    for match in re.finditer(
            r"(?:static\s+)?const\s+union\s+AffineAnimCmd\s+(\w+)\[\]\s*=\s*"
            r"\{(.*?)\};", source, re.S):
        raw_commands: list[dict[str, int | str]] = []
        for command_match in re.finditer(
                r"AFFINEANIMCMD_(FRAME|LOOP|JUMP|END(?:_ALT)?)\s*"
                r"(?:\(((?:[^()]|\([^()]*\))*)\))?", match.group(2), re.S):
            kind = command_match.group(1)
            operands = split_operands(command_match.group(2) or "")
            if kind == "FRAME" and len(operands) >= 4:
                raw_commands.append({
                    "kind": kind,
                    "x": _parse_source_int(operands[0]),
                    "y": _parse_source_int(operands[1]),
                    "rotation": _parse_source_int(operands[2]),
                    "duration": _parse_source_int(operands[3]),
                })
            elif kind in {"LOOP", "JUMP"} and operands:
                raw_commands.append({"kind": kind,
                                     "value": _parse_source_int(operands[0])})
            elif kind.startswith("END"):
                raw_commands.append({"kind": "END"})
        commands: list[dict[str, int]] = []
        command_to_output: dict[int, int] = {}
        loop_start = 0
        infinite_loop_command = 0xFF
        for command_index, command in enumerate(raw_commands):
            command_to_output[command_index] = len(commands)
            kind = command["kind"]
            if kind == "FRAME":
                commands.append({key: int(value) for key, value in command.items()
                                 if key != "kind"})
            elif kind == "LOOP":
                count = int(command["value"])
                if count == 0:
                    loop_start = len(commands)
                elif loop_start < len(commands):
                    segment = [dict(value) for value in commands[loop_start:]]
                    for _ in range(count):
                        commands.extend(dict(value) for value in segment)
            elif kind == "JUMP":
                infinite_loop_command = command_to_output.get(
                    int(command["value"]), 0)
                break
            elif kind == "END":
                break
        if len(commands) > 255:
            raise RuntimeError(
                f"Expanded affine sequence is too long: {match.group(1)} ({len(commands)})")
        if _source_name == "expansion" and match.group(1) in affine_command_arrays:
            continue
        affine_command_arrays[match.group(1)] = {
            "commands": commands,
            "loopCommand": infinite_loop_command,
        }
    for match in re.finditer(
        r"(?:static\s+)?const\s+union\s+AffineAnimCmd\s*\*\s*const\s+"
            r"(\w+)\[\]\s*=\s*(?://[^\r\n]*)?\s*\{(.*?)\};", source, re.S):
        if _source_name == "expansion" and match.group(1) in affine_pointer_tables:
            continue
        affine_pointer_tables[match.group(1)] = re.findall(
            r"^\s*(\w+)\s*,?\s*$", match.group(2), re.M)

# The expansion has one legacy raw-OAM template instead of a named
# gOamData_* declaration.  Its attr0/attr1 shape+size bits are 0/0, i.e. one
# 8x8 square cell; keep the decoded contract explicit so a future raw OAM
# addition still stops generation for review.
RAW_TEMPLATE_OAM_SIZES: dict[str, tuple[int, int]] = {
    "gPetalBlizzardTwister2Template": (8, 8),
}

template_specs: dict[str, dict[str, object]] = {}
for source, source_name in animation_sources:
    for match in re.finditer(
            r"(?:const|static const)\s+struct\s+SpriteTemplate\s+(\w+)\s*=\s*\{(.*?)\};",
            source, re.S):
        if source_name == "expansion" and match.group(1) in template_specs:
            # The expansion intentionally carries the same GBA templates for
            # legacy moves. Keep FireRed's already audited definition for a
            # shared symbol; only genuinely new Gen-IV templates are added.
            continue
        body = match.group(2)
        tag = re.search(r"\.tileTag\s*=\s*ANIM_TAG_([A-Z0-9_]+)", body)
        callback = re.search(r"\.callback\s*=\s*(\w+)", body)
        palette_tag = re.search(
            r"\.paletteTag\s*=\s*ANIM_TAG_([A-Z0-9_]+)", body)
        animations = re.search(r"\.anims\s*=\s*(\w+)", body)
        affine_animations = re.search(r"\.affineAnims\s*=\s*(\w+)", body)
        oam_size = re.search(r"gOamData_[A-Za-z0-9_]+_(\d+)x(\d+)", body)
        raw_width, raw_height = RAW_TEMPLATE_OAM_SIZES.get(
            match.group(1), (0, 0))
        template_specs[match.group(1)] = {
            "tag": (tag.group(1) if tag else
                    "SMOKESCREEN_IMPACT" if match.group(1) ==
                    "sSmokescreenImpactSpriteTemplate" else ""),
            "paletteTag": palette_tag.group(1) if palette_tag else
                          (tag.group(1) if tag else ""),
            "source": source_name,
            "callback": callback.group(1) if callback else "",
            "animations": animations.group(1) if animations else "",
            "affineAnimations": (affine_animations.group(1)
                                 if affine_animations else ""),
            "width": (int(oam_size.group(1)) if oam_size else
                      16 if match.group(1) == "sSmokescreenImpactSpriteTemplate"
                      else raw_width),
            "height": (int(oam_size.group(2)) if oam_size else
                       16 if match.group(1) == "sSmokescreenImpactSpriteTemplate"
                       else raw_height),
        }


# Callbacks which cannot be inferred from one of FireRed's standard movement
# helpers. Each entry is a reviewed retained renderer, including callbacks
# whose sprite is deliberately static because an owning visual task supplies
# its coordinates. Keeping this table explicit prevents names such as "Orb"
# or "Wave" from silently choosing a wrong generic trajectory.
AUDITED_GENERIC_SPRITE_CALLBACKS: dict[str, tuple[str, int]] = {
    "AnimSlideHandOrFootToTarget": ("BattleAnimationMotion::DiagonalStrike", 18),
    "AnimWhirlwindLine": ("BattleAnimationMotion::RoarLine", 18),
    "AnimFlyBallUp": ("BattleAnimationMotion::Rising", 28),
    "AnimJumpKick": ("BattleAnimationMotion::Projectile", 18),
    "AnimDragonFireToTarget": ("BattleAnimationMotion::DragonFire", 21),
    "AnimElectricBoltSegment": ("BattleAnimationMotion::Static", 16),
    "AnimThunderboltOrb": ("BattleAnimationMotion::ThunderboltOrbBlink", 64),
    "AnimThunderWave": ("BattleAnimationMotion::ThunderWavePairBlink", 51),
    "SpriteCallbackDummy": ("BattleAnimationMotion::Static", 18),
    # AnimTask_CreateIons owns each spawn coordinate and the +1/+4 drift in
    # the retained IonRain renderer. AnimIon itself only installs the child
    # callback, so the exported sprite resource must remain position-neutral.
    "AnimIon": ("BattleAnimationMotion::Static", 18),
    "AnimSkyAttackBird": ("BattleAnimationMotion::SkyAttackBird", 30),
    "AnimMovementWaves": ("BattleAnimationMotion::MovementWavesRepeat", 96),
    "AnimReversalOrb": ("BattleAnimationMotion::FastOrbit", 52),
    "AnimSludgeBombHitParticle": ("BattleAnimationMotion::Scatter", 18),
    "AnimWaveFromCenterOfTarget": ("BattleAnimationMotion::Static", 18),
    "AnimSparkElectricity": ("BattleAnimationMotion::StaticElectricity", 64),
    "AnimPainSplitProjectile": ("BattleAnimationMotion::PainSplitBounce", 28),
    # Args 0/1/2 are the attacker->centre, centre hold and centre->target
    # durations. A generic Projectile incorrectly treats args 0/1 as screen
    # offsets and, because the callback changes state midway, fails travel
    # inference altogether. Keep the callback as its own retained motion.
    "AnimShadowBall": ("BattleAnimationMotion::ShadowBall", 41),
    "AnimSwallowBlueOrb": ("BattleAnimationMotion::Rising", 30),
    "AnimGrowingChargeOrb": ("BattleAnimationMotion::Static", 172),
    "AnimNeedleArmSpike": ("BattleAnimationMotion::Projectile", 18),
    "AnimDiveBall": ("BattleAnimationMotion::DiveBallCycle", 96),
    "AnimTailGlowOrb": ("BattleAnimationMotion::TailGlowOrbExact", 115),
    "AnimWeatherBallUp": ("BattleAnimationMotion::Rising", 32),
    "AnimAirWaveProjectile": ("BattleAnimationMotion::Projectile", 38),
    "AnimSmallWaterOrb": ("BattleAnimationMotion::Scatter", 24),
    "AnimBounceBallShrink": ("BattleAnimationMotion::BounceBallShrinkExact", 24),
    "AnimBounceBallLand": ("BattleAnimationMotion::BlockBounce", 36),
    "AnimVoltTackleOrbSlide": ("BattleAnimationMotion::VoltTackleSlide", 66),
    "AnimVoltTackleBolt": ("BattleAnimationMotion::Static", 16),
    "AnimRockBlastRock": ("BattleAnimationMotion::TargetLocationProjectile", 26),
    # These expansion callbacks are position/affine state machines, not
    # generic projectiles. Their exact source coordinates are retained by the
    # generated origin and affine streams below.
    "AnimAquaTail": ("BattleAnimationMotion::Static", 32),
    "AnimateZenHeadbutt": ("BattleAnimationMotion::ZenHeadbuttExact", 64),
    "AnimShakeMonOrBattlePlatforms": ("BattleAnimationMotion::BattlerShake", 24),
    "AnimGrowingShockWaveOrb": ("BattleAnimationMotion::Static", 172),
    "AnimShockWaveProgressingBolt": ("BattleAnimationMotion::Static", 16),
    "AnimPsychoBoost": ("BattleAnimationMotion::Rising", 30),
    "AnimEmberFlare": ("BattleAnimationMotion::Projectile", 18),
    "AnimThrowMistBall": ("BattleAnimationMotion::TargetLocationProjectile", 31),
    "AnimRainDrop": ("BattleAnimationMotion::Falling", 18),
    "AnimSuperpowerOrb": ("BattleAnimationMotion::Projectile", 20),
    "AnimWavyMusicNotes": ("BattleAnimationMotion::Wave", 36),
    # Expansion-only Gen-IV callbacks. Their source state machines are
    # translated to explicit retained motions with the same travel direction;
    # they never fall through to a name-based placeholder.
    "AnimMoveAccupressure": ("BattleAnimationMotion::StandardLinearProjectile", 31),
    "AnimMoveFeintSwipe": ("BattleAnimationMotion::SliceArc", 31),
    # Trump Card initializes at the target and then runs its own 80px
    # horizontal arc; its arguments are not the public projectile ABI.
    "AnimMoveTrumpCard": ("BattleAnimationMotion::TrumpCardArc", 31),
    # Poison Jab starts from an explicit offset around the target and uses
    # arg2 as its travel duration, not the standard five-argument layout.
    "AnimPoisonJabProjectile": ("BattleAnimationMotion::PoisonJabProjectile", 31),
    "AnimWoodHammerSmall": ("BattleAnimationMotion::TargetLocalLinear", 31),
    "AvalancheAnim_Step": ("BattleAnimationMotion::FallingRockEllipse", 64),
    "AnimKnockOffAquaTail": ("BattleAnimationMotion::AquaTailSweep", 32),
    "AnimDracoMeteorRock": ("BattleAnimationMotion::DracoMeteorRock", 64),
    "AnimDragonRush": ("BattleAnimationMotion::DragonRushStrike", 8),
    "AnimMoveSmallCloud": ("BattleAnimationMotion::SmallCloudDrift", 64),
    "SpriteCB_SpriteOnMonForDuration": ("BattleAnimationMotion::SpriteOnMonDuration", 64),
    # These callbacks only place a source-authored affine/cel sequence. The
    # motion is static, but the callback still owns battler selection and the
    # generated affine stream must remain intact.
    "AnimMoveFeintZoom": ("BattleAnimationMotion::Static", 64),
    "AnimForcePalm": ("BattleAnimationMotion::Static", 64),
    "AnimGunkShotImpact": ("BattleAnimationMotion::Static", 64),
    "AnimPunishment": ("BattleAnimationMotion::Static", 64),
    "AnimRockPolishSparkle": ("BattleAnimationMotion::Static", 32),
    # Reflect Type and later expansion moves place a guard ring 40px below
    # the attacker and linearly raise it 72px over thirteen frames.
    "SpriteCB_SurroundingRing": ("BattleAnimationMotion::RaiseSprite", 14),
    # Water Pledge starts at the target and rises by arg3 pixels per frame
    # until reaching arg2. The retained Rising profile preserves that visible
    # target-local vertical travel instead of freezing the orb at spawn.
    "AnimFlyUpTarget": ("BattleAnimationMotion::Rising", 48),
    # Reflect Type reuses String Shot's exact three-frame blink cadence and
    # 51-frame lifetime; only the initializer name differs in expansion.
    "SpriteCB_ToxicThreadWrap": ("BattleAnimationMotion::StringWrapBlink", 51),
    # Searing Shot uses the same hold-then-frozen-affine state machine as the
    # canonical spinning kick/punch callback, with arg4 only selecting the
    # affected battler in doubles (the app's battle scene is single-target).
    "SpriteCB_SearingShotRock": ("BattleAnimationMotion::SpinningKickOrPunchExact", 96),
}

# Stationary is a valid FireRed contract only for callbacks reviewed here.
# Keeping this proof separate from the motion-name heuristics lets the audit
# reject every new callback which would otherwise silently become a static
# cel merely because its state machine was not understood.
REVIEWED_STATIC_CALLBACKS = {
    callback for callback, (motion, _) in
    AUDITED_GENERIC_SPRITE_CALLBACKS.items()
    if motion == "BattleAnimationMotion::Static"
} | {
    "AnimClawSlash",
    "AnimElectricPuff",
    "AnimEruptionLaunchRock",
    "AnimFang",
    "AnimFurySwipes",
    "AnimGlareEyeDot",
    "AnimHitSplatOnMonEdge",
    "AnimSpeedDust",
    "AnimSuperFang",
    "SpriteCB_SmokescreenImpact",
}


def sprite_motion(template: str, callback: str, tag: str) -> tuple[str, int]:
    # The initializer is safe for generic helper discovery. Installed state
    # callbacks are handled by explicit semantic profiles below; feeding the
    # whole chain through keyword heuristics would mistake a late cleanup
    # translation for the primary motion (Swords Dance is a classic example).
    body = expanded_callback_body(callback).lower()
    key = f"{template} {callback} {body}".lower()
    # These dummy sprites do not draw an OBJ of their own.  Their callbacks
    # translate the selected battler through x2/y2 using a distinct ABI.  The
    # retained renderer must keep the three profiles separate so a duration
    # or mirror flag is never mistaken for a screen offset.
    if template in {"gSlideMonToOffsetSpriteTemplate",
                    "gSlideMonToOffsetPartnerSpriteTemplate"}:
        return "BattleAnimationMotion::BattlerSlideToOffset", 16
    if template in {"gSlideMonToOriginalPosSpriteTemplate",
                    "gSlideMonToOriginalPosPartnerSpriteTemplate"}:
        return "BattleAnimationMotion::BattlerRestorePosition", 16
    if template == "gSlideMonToOffsetAndBackSpriteTemplate":
        return "BattleAnimationMotion::BattlerSlideToOffsetAndBack", 16
    # This public FireRed helper has a six-argument ABI that the broad
    # Projectile renderer cannot safely approximate.  In particular arg 4 is
    # the travel duration and args 2/3 are destination offsets.
    if callback == "TranslateAnimSpriteToTargetMonLocation":
        return "BattleAnimationMotion::TargetLocationProjectile", 31
    standard_linear_callbacks = {
        "AnimAirWaveCrescent",
        "AnimAuroraBeamRings",
        "AnimGustToTarget",
        "AnimHydroCannonBeam",
        "AnimSonicBoomProjectile",
        "AnimTranslateStinger",
    }
    if callback in standard_linear_callbacks:
        return "BattleAnimationMotion::StandardLinearProjectile", 31
    standard_arc_callbacks = {
        "AnimMissileArc",
        "AnimThrowProjectile",
        "AnimTranslateLinearSingleSineWave",
        "InitIceBallAnim",
    }
    if callback in standard_arc_callbacks:
        return "BattleAnimationMotion::StandardArcProjectile", 31
    # These callbacks all call a standard translation helper at some point,
    # but the helper is only one part of their real state machine.  Treating
    # that call as proof of a plain Projectile discarded the wave/orbit,
    # target bounce, fade, or second motion phase.  Keep each source ABI
    # explicit and let the firmware replay the callback's actual formula.
    nonlinear_translation_callbacks = {
        "AnimToTargetInSinWave": ("BattleAnimationMotion::SinWaveProjectile", 31),
        "AnimHyperBeamOrb": ("BattleAnimationMotion::HyperBeamOrb", 31),
        "AnimConfuseRayBallBounce": ("BattleAnimationMotion::ConfuseRayBounce", 42),
        "AnimBulletSeed": ("BattleAnimationMotion::BulletSeedRicochet", 37),
        "AnimTranslateWebThread": ("BattleAnimationMotion::WebThreadWave", 36),
        "AnimWillOWispOrb": ("BattleAnimationMotion::WillOWispOrb", 64),
        "AnimRedHeartProjectile": ("BattleAnimationMotion::HeartWaveProjectile", 96),
        "AnimSlidingKick": ("BattleAnimationMotion::SlidingKickWave", 31),
        "AnimSlowFlyingMusicNotes": ("BattleAnimationMotion::MusicNoteWave", 41),
        "AnimSolarBeamSmallOrb": ("BattleAnimationMotion::SolarBeamOrb", 81),
        "AnimZapCannonSpark": ("BattleAnimationMotion::ZapCannonSpark", 31),
        "InitPoisonGasCloudAnim": ("BattleAnimationMotion::PoisonGasCloud", 160),
        "InitSwirlingFogAnim": ("BattleAnimationMotion::SwirlingFog", 64),
    }
    explicit_nonlinear = nonlinear_translation_callbacks.get(callback)
    if explicit_nonlinear is not None:
        return explicit_nonlinear
    # These callbacks invoke one of the shared GBA translation helpers only
    # after arranging callback-specific coordinates/state.  They previously
    # fell through to Projectile/Arc, so duration, frame selector and battler
    # selector arguments were routinely interpreted as x/y offsets.  Every
    # entry below has a matching source-level evaluator in the firmware.
    helper_state_callbacks = {
        "AnimAbsorptionOrb": ("BattleAnimationMotion::AbsorptionArc", 40),
        "AnimAcidPoisonBubble": ("BattleAnimationMotion::AcidBubbleArc", 64),
        "AnimAcidPoisonDroplet": ("BattleAnimationMotion::TargetDroplet", 64),
        "AnimBoneHitProjectile": ("BattleAnimationMotion::TargetLocalLinear", 64),
        "AnimBonemerangProjectile": ("BattleAnimationMotion::Bonemerang", 42),
        "AnimCoinThrow": ("BattleAnimationMotion::CoinThrow", 64),
        "AnimCrossChopHand": ("BattleAnimationMotion::CrossChop", 48),
        "AnimDirtPlumeParticle": ("BattleAnimationMotion::DirtPlume", 64),
        "AnimFireCross": ("BattleAnimationMotion::LocalFixedVelocity", 64),
        "AnimFireSpread": ("BattleAnimationMotion::LocalFixedVelocity", 64),
        "SpriteCB_LockingJaw": ("BattleAnimationMotion::LockingJaw", 40),
        "AnimFlyBallAttack": ("BattleAnimationMotion::OffscreenFly", 64),
        "AnimGrantingStars": ("BattleAnimationMotion::LocalStarVelocity", 64),
        "AnimSparklingStars": ("BattleAnimationMotion::LocalStarVelocity", 64),
        "AnimHyperVoiceRing": ("BattleAnimationMotion::HyperVoiceRing", 64),
        "AnimLeechSeed": ("BattleAnimationMotion::LeechSeedArc", 96),
        "AnimMimicOrb": ("BattleAnimationMotion::MimicOrb", 64),
        "AnimPowerAbsorptionOrb": ("BattleAnimationMotion::AbsorptionLinear", 64),
        "AnimRaiseSprite": ("BattleAnimationMotion::RaiseSprite", 64),
        "AnimRolloutParticle": ("BattleAnimationMotion::PreinitializedArc", 48),
        "AnimSkillSwapOrb": ("BattleAnimationMotion::PreinitializedArc", 64),
        "AnimSludgeProjectile": ("BattleAnimationMotion::SludgeArc", 64),
        "AnimSolarBeamBigOrb": ("BattleAnimationMotion::SolarBeamBigOrb", 96),
        "AnimSpikes": ("BattleAnimationMotion::SpikesArc", 96),
        "AnimSunlight": ("BattleAnimationMotion::SunlightRay", 61),
        "AnimSuperpowerFireball": ("BattleAnimationMotion::SuperpowerFireball", 17),
        "AnimTearDrop": ("BattleAnimationMotion::TearDropArc", 33),
        "AnimTravelDiagonally": ("BattleAnimationMotion::TravelDiagonal", 64),
        "AnimTriAttackTriangle": ("BattleAnimationMotion::TriAttackTriangle", 82),
        "AnimWaterGunDroplet": ("BattleAnimationMotion::WaterGunDroplet", 64),
    }
    helper_profile = helper_state_callbacks.get(callback)
    if helper_profile is not None:
        return helper_profile
    particle_state_callbacks = {
        "AnimFistOrFootRandomPos": ("BattleAnimationMotion::RandomBattlerHit", 24),
        "AnimHitSplatRandom": ("BattleAnimationMotion::RandomBattlerHit", 24),
        "AnimFlatterConfetti": ("BattleAnimationMotion::ConfettiBallistic", 31),
        "AnimFlyingParticle": ("BattleAnimationMotion::FlyingParticle", 96),
        "AnimGreenStar": ("BattleAnimationMotion::GreenStarRise", 96),
        "AnimMovePowderParticle": ("BattleAnimationMotion::PowderParticle", 96),
        "AnimMoveTwisterParticle": ("BattleAnimationMotion::TwisterParticle", 96),
        "AnimPresentHealParticle": ("BattleAnimationMotion::PresentHealRise", 32),
        "AnimRazorLeafParticle": ("BattleAnimationMotion::RazorLeafDrift", 128),
        "AnimRockFragment": ("BattleAnimationMotion::RockFragmentLinear", 96),
        "AnimRockScatter": ("BattleAnimationMotion::RockScatterBounce", 19),
        "AnimSmallDriftingBubbles": ("BattleAnimationMotion::DriftingBubble", 21),
        "InitIceBallParticle": ("BattleAnimationMotion::DriftingBubble", 21),
        "AnimSporeParticle": ("BattleAnimationMotion::SporeOrbit", 96),
        "AnimDizzyPunchDuck": ("BattleAnimationMotion::DizzyWave", 42),
        "AnimFocusPunchFist": ("BattleAnimationMotion::FocusPunchWobble", 48),
        "AnimGhostStatusSprite": ("BattleAnimationMotion::GhostRiseFade", 48),
        "AnimGrudgeFlame": ("BattleAnimationMotion::GrudgeFlameWave", 64),
        "AnimLetterZ": ("BattleAnimationMotion::SleepLetterWave", 64),
        "AnimMagentaHeart": ("BattleAnimationMotion::RisingHeartWave", 60),
        "AnimParticleBurst": ("BattleAnimationMotion::ParticleBurstWave", 42),
        "AnimPinkHeart": ("BattleAnimationMotion::PinkHeartWave", 102),
        "AnimBatonPassPokeball": ("BattleAnimationMotion::BatonPassBall", 42),
        "AnimEruptionFallingRock": ("BattleAnimationMotion::EruptionFallingRock", 96),
        "AnimFacadeSweatDrop": ("BattleAnimationMotion::ShortLinearDrop", 7),
        "AnimFallingCoin": ("BattleAnimationMotion::FallingCoinBounce", 52),
        "AnimFallingFeather": ("BattleAnimationMotion::FallingFeatherWave", 128),
        "AnimFallingRock": ("BattleAnimationMotion::FallingRockEllipse", 64),
        "AnimHailBegin": ("BattleAnimationMotion::HailDiagonal", 48),
        "AnimIngrainOrb": ("BattleAnimationMotion::IngrainOrbWave", 96),
        "AnimIngrainRoot": ("BattleAnimationMotion::IngrainRoot", 64),
        "AnimMudSportDirt": ("BattleAnimationMotion::MudSportDirt", 64),
        "AnimRedHeartRising": ("BattleAnimationMotion::RedHeartRising", 96),
        "AnimSprayWaterDroplet": ("BattleAnimationMotion::WaterSprayBallistic", 31),
        "AnimAirWaveProjectile": ("BattleAnimationMotion::AirWaveTaskProjectile", 56),
        "AnimDiveBall": ("BattleAnimationMotion::DiveBallCycle", 96),
        "AnimFlyBallUp": ("BattleAnimationMotion::FlyBallRise", 64),
        "AnimEmberFlare": ("BattleAnimationMotion::TravelDiagonal", 64),
        "AnimNeedleArmSpike": ("BattleAnimationMotion::NeedleArmSpike", 64),
        "AnimPsychoBoost": ("BattleAnimationMotion::PsychoBoostRise", 48),
        "AnimRainDrop": ("BattleAnimationMotion::RainDrop", 16),
        "AnimSludgeBombHitParticle": ("BattleAnimationMotion::SludgeBurst", 64),
        "AnimSmallWaterOrb": ("BattleAnimationMotion::SmallWaterOrb", 64),
        "AnimSuperpowerOrb": ("BattleAnimationMotion::SuperpowerOrbHold", 197),
        "AnimSwallowBlueOrb": ("BattleAnimationMotion::SwallowOrb", 64),
        "AnimWavyMusicNotes": ("BattleAnimationMotion::WavyMusicNote", 41),
        "AnimWeatherBallUp": ("BattleAnimationMotion::WeatherBallRise", 64),
    }
    particle_profile = particle_state_callbacks.get(callback)
    if particle_profile is not None:
        return particle_profile
    # Circular-looking callbacks do not share an argument ABI in FireRed.
    # Keep every used family explicit: the former broad Sin+Cos heuristic
    # interpreted angles, radii and fixed-point speeds as screen offsets,
    # visibly displacing Fire Spin and corrupting the other effects below.
    circular_callbacks = {
        "AnimParticleInVortex": ("BattleAnimationMotion::Vortex", 30),
        "AnimFireSpiralInward": ("BattleAnimationMotion::GrowingCircle", 30),
        "AnimIcePunchSwirlingParticle": ("BattleAnimationMotion::GrowingCircle", 30),
        "AnimRazorWindTornado": ("BattleAnimationMotion::Circular", 30),
        "AnimWaterBubbleProjectile": ("BattleAnimationMotion::OrbitingProjectile", 50),
        "AnimConfuseRayBallSpiral": ("BattleAnimationMotion::ConfuseRaySpiral", 61),
        "AnimOrbitFast": ("BattleAnimationMotion::FastOrbit", 52),
        "AnimOrbitScatter": ("BattleAnimationMotion::RadialScatter", 32),
        "AnimSpitUpOrb": ("BattleAnimationMotion::RadialScatter", 24),
        "AnimFireSpiralOutward": ("BattleAnimationMotion::ExpandingSpiral", 30),
        "AnimWillOWispFire": ("BattleAnimationMotion::ExpandingEllipse", 30),
        "AnimOverheatFlame": ("BattleAnimationMotion::RadialLinear", 30),
        "AnimDragonDanceOrb": ("BattleAnimationMotion::DragonDanceOrbit", 82),
        "AnimAngel": ("BattleAnimationMotion::AngelSway", 101),
        "AnimDevil": ("BattleAnimationMotion::DevilOrbit", 91),
        "AnimPerishSongMusicNote": ("BattleAnimationMotion::PerishSongNote", 120),
        "AnimTrickBag": ("BattleAnimationMotion::TrickBagOrbit", 64),
        # These affine cels remain centered on the battler selected by arg 2;
        # their artwork animates, but their callback never moves in an orbit.
        "AnimSpriteOnMonPos": ("BattleAnimationMotion::OrientedStatic", 20),
    }
    explicit_circular = circular_callbacks.get(callback)
    if explicit_circular is not None:
        return explicit_circular
    # SpriteTemplate initializers commonly install the real per-frame state
    # machine. Keep those families callback-explicit; their argument layouts
    # are not interchangeable with Projectile/Arc/Orbit and a name heuristic
    # cannot reproduce them safely.
    state_machine_callbacks = {
        # Stationary in world space does not mean callback-free.  These
        # callbacks still own side mirroring, a delayed sequence, visibility,
        # alpha, a timer, or a task signal which cannot be inferred from the
        # SpriteTemplate's cel table alone.
        "AnimBasicFistOrFoot": ("BattleAnimationMotion::BasicFistOrFootExact", 64),
        "AnimSpinningKickOrPunch": ("BattleAnimationMotion::SpinningKickOrPunchExact", 96),
        "AnimHitSplatHandleInvert": ("BattleAnimationMotion::InvertedHitSplatExact", 16),
        "AnimWhipHit": ("BattleAnimationMotion::WhipHitExact", 32),
        "AnimBentSpoon": ("BattleAnimationMotion::BentSpoonExact", 187),
        "AnimQuestionMark": ("BattleAnimationMotion::QuestionMarkExact", 102),
        "AnimDigDirtMound": ("BattleAnimationMotion::DigDirtMoundExact", 96),
        "AnimBellyDrumHand": ("BattleAnimationMotion::BellyDrumHandExact", 9),
        "AnimPerishSongMusicNote2": ("BattleAnimationMotion::PerishSongDelayedNote", 200),
        "AnimMovementWaves": ("BattleAnimationMotion::MovementWavesRepeat", 96),
        "AnimFlyingSandCrescent": ("BattleAnimationMotion::FlyingSandCrescent", 192),
        "AnimWeakFrustrationAngerMark": ("BattleAnimationMotion::WeakFrustrationMark", 96),
        "AnimClappingHand2": ("BattleAnimationMotion::ClappingHandWindowMask", 36),
        "AnimRevengeScratch": ("BattleAnimationMotion::RevengeScratchExact", 24),
        "AnimHitSplatPersistent": ("BattleAnimationMotion::HitSplatPersistentExact", 128),
        "AnimDiveWaterSplash": ("BattleAnimationMotion::DiveWaterSplash", 25),
        "AnimTailGlowOrb": ("BattleAnimationMotion::TailGlowOrbExact", 115),
        "AnimHydroCannonCharge": ("BattleAnimationMotion::HydroCannonChargeExact", 80),
        "AnimBounceBallShrink": ("BattleAnimationMotion::BounceBallShrinkExact", 24),
        "AnimAssistPawprint": ("BattleAnimationMotion::AssistPawprintTravel", 37),
        "AnimArmThrustHit": ("BattleAnimationMotion::ArmThrustImpact", 24),
        "AnimDefensiveWall": ("BattleAnimationMotion::DefensiveWallCycle", 62),
        "AnimConstrictBinding": ("BattleAnimationMotion::ConstrictBinding", 255),
        "AnimFalseSwipePositionedSlice": ("BattleAnimationMotion::FalseSwipeFlicker", 22),
        "AnimFlashingHitSplat": ("BattleAnimationMotion::FlashingHitFlicker", 14),
        "AnimFlatterSpotlight": ("BattleAnimationMotion::FlatterSpotlightMask", 123),
        "AnimFrenzyPlantRoot": ("BattleAnimationMotion::FrenzyPlantRoot", 64),
        "AnimLick": ("BattleAnimationMotion::LickFlicker", 51),
        "AnimMetronomeFinger": ("BattleAnimationMotion::MetronomeFinger", 74),
        "AnimMilkBottle": ("BattleAnimationMotion::MilkBottleDrink", 108),
        "AnimRecycle": ("BattleAnimationMotion::RecycleFade", 139),
        "AnimRedX": ("BattleAnimationMotion::TimedRedX", 64),
        "AnimSharpenSphere": ("BattleAnimationMotion::SharpenBlink", 192),
        "AnimSmellingSaltExclamation": ("BattleAnimationMotion::SmellingSaltsBlink", 64),
        "AnimSpiderWeb": ("BattleAnimationMotion::SpiderWebFade", 53),
        "AnimStringWrap": ("BattleAnimationMotion::StringWrapBlink", 51),
        "AnimTauntFinger": ("BattleAnimationMotion::TauntFinger", 47),
        "AnimThoughtBubble": ("BattleAnimationMotion::ThoughtBubble", 64),
        "AnimWhiteHalo": ("BattleAnimationMotion::WhiteHaloFade", 99),
        "AnimThunderboltOrb": ("BattleAnimationMotion::ThunderboltOrbBlink", 64),
        "AnimThunderWave": ("BattleAnimationMotion::ThunderWavePairBlink", 51),
        "AnimSparkElectricity": ("BattleAnimationMotion::StaticElectricity", 64),
        "InitIceBallParticle": ("BattleAnimationMotion::IceBallImpactShard", 21),
        "AnimSlashSlice": ("BattleAnimationMotion::SlashFlicker", 34),
        "AnimNightSlash": ("BattleAnimationMotion::SlashFlicker", 34),
        "AnimConversion": ("BattleAnimationMotion::ConversionHold", 255),
        "AnimMoon": ("BattleAnimationMotion::MoonAbsolute", 255),
        "AnimMoonlightSparkle": ("BattleAnimationMotion::MoonlightSparkleFall", 255),
        "AnimBreathPuff": ("BattleAnimationMotion::BreathPuff", 53),
        "AnimGuardRing": ("BattleAnimationMotion::GuardRingRise", 14),
        "AnimViceGripPincer": ("BattleAnimationMotion::ViceGripPincer", 26),
        "AnimGuillotinePincer": ("BattleAnimationMotion::GuillotinePincer", 70),
        "AnimHealBellMusicNote": ("BattleAnimationMotion::HealBellNote", 36),
        "AnimJumpKick": ("BattleAnimationMotion::DiagonalStrike", 18),
        "AnimMegahornHorn": ("BattleAnimationMotion::MegahornStrike", 7),
        "AnimLeechLifeNeedle": ("BattleAnimationMotion::LeechLifeNeedle", 13),
        "AnimDirtScatter": ("BattleAnimationMotion::DirtProjectile", 18),
        "AnimOutrageFlame": ("BattleAnimationMotion::OutrageFlame", 31),
        "AnimPetalDanceBigFlower": ("BattleAnimationMotion::PetalDanceBig", 141),
        "AnimPetalDanceSmallFlower": ("BattleAnimationMotion::PetalDanceSmall", 101),
        "AnimTealAlert": ("BattleAnimationMotion::TargetConverge", 7),
        "AnimWeatherBallDown": ("BattleAnimationMotion::WeatherBallDown", 26),
        "AnimSwordsDanceBlade": ("BattleAnimationMotion::SwordRise", 18),
        "AnimCuttingSlice": ("BattleAnimationMotion::SliceArc", 23),
        "AnimAirCutterSlice": ("BattleAnimationMotion::SliceArc", 23),
        "AnimEllipticalGust": ("BattleAnimationMotion::EllipticalGust", 71),
        "AnimStompFoot": ("BattleAnimationMotion::StompDrop", 23),
        "AnimHornHit": ("BattleAnimationMotion::HornLunge", 18),
        "AnimBubbleEffect": ("BattleAnimationMotion::BubbleRise", 30),
        "AnimBite": ("BattleAnimationMotion::BiteClamp", 16),
        "AnimRoarNoiseLine": ("BattleAnimationMotion::RoarLine", 14),
        "AnimSparkElectricityFlashing": ("BattleAnimationMotion::ElectricOrbit", 32),
        "AnimBlackSmoke": ("BattleAnimationMotion::SmokeDrift", 28),
        "AnimEndureEnergy": ("BattleAnimationMotion::EndureRise", 28),
        "AnimFireRing": ("BattleAnimationMotion::FireRing", 74),
        "AnimSmallBubblePair": ("BattleAnimationMotion::BubbleRise", 30),
        "AnimSoftBoiledEgg": ("BattleAnimationMotion::SoftBoiledEgg", 64),
        "AnimSleepLetterZ": ("BattleAnimationMotion::SleepLetter", 64),
        "AnimPencil": ("BattleAnimationMotion::PencilScribble", 68),
        "AnimCurseNail": ("BattleAnimationMotion::CurseNail", 76),
        "AnimConversion2": ("BattleAnimationMotion::ConversionReturn", 166),
        "AnimProtect": ("BattleAnimationMotion::ProtectSlide", 48),
        "AnimForesightMagnifyingGlass": ("BattleAnimationMotion::ForesightScan", 90),
        "AnimDestinyBondWhiteShadow": ("BattleAnimationMotion::DestinyShadow", 48),
        "AnimLockOnTarget": ("BattleAnimationMotion::LockOnScan", 58),
        "AnimLockOnMoveTarget": ("BattleAnimationMotion::LockOnScan", 58),
        "AnimFalseSwipeSlice": ("BattleAnimationMotion::FalseSwipe", 30),
        "AnimMeanLookEye": ("BattleAnimationMotion::SpotlightSway", 48),
        "AnimPresent": ("BattleAnimationMotion::PresentBounce", 34),
        "AnimFirePlume": ("BattleAnimationMotion::FlameDrift", 36),
        "AnimLargeFlame": ("BattleAnimationMotion::FlameDrift", 36),
        "AnimSpotlight": ("BattleAnimationMotion::SpotlightSway", 64),
        "AnimClappingHand": ("BattleAnimationMotion::ClappingHands", 36),
        "AnimRapidSpin": ("BattleAnimationMotion::RapidSpin", 36),
        "AnimSweetScentPetal": ("BattleAnimationMotion::SweetScentPetal", 52),
        "AnimJaggedMusicNote": ("BattleAnimationMotion::RoarLine", 18),
        "AnimSmellingSaltsHand": ("BattleAnimationMotion::SmellingSalts", 54),
        "AnimFollowMeFinger": ("BattleAnimationMotion::FollowMeFinger", 64),
        "AnimHelpingHandClap": ("BattleAnimationMotion::HelpingHandClap", 92),
        "AnimWishStar": ("BattleAnimationMotion::WishStar", 48),
        "AnimSuperpowerRock": ("BattleAnimationMotion::SuperpowerRock", 44),
        "AnimBrickBreakWall": ("BattleAnimationMotion::BrickWallShake", 36),
        "AnimBrickBreakWallShard": ("BattleAnimationMotion::BrickWallShard", 41),
        "AnimYawnCloud": ("BattleAnimationMotion::YawnCloud", 64),
        "AnimKnockOffStrike": ("BattleAnimationMotion::KnockOffStrike", 34),
        "AnimFlyingMusicNotes": ("BattleAnimationMotion::MusicOrbit", 48),
        "AnimMeteorMashStar": ("BattleAnimationMotion::MeteorMashStar", 28),
        "AnimRockTomb": ("BattleAnimationMotion::RockTombBounce", 38),
        "AnimBlockX": ("BattleAnimationMotion::BlockBounce", 74),
        "AnimWaterPulseBubble": ("BattleAnimationMotion::WaterPulseBubble", 38),
        "AnimWaterPulseRing": ("BattleAnimationMotion::WaterPulseRing", 34),
        "SpriteCB_MaxFlutterby": ("BattleAnimationMotion::MaxFlutterbyArc", 16),
        "AnimMoveWringOut": ("BattleAnimationMotion::WringOutCircle", 80),
        "AnimWoodHammerHammer": ("BattleAnimationMotion::WoodHammerHammer", 84),
        "AnimMovePowerSwapGuardSwap": ("BattleAnimationMotion::PowerSwapGuardSwapArc", 31),
        "AnimGrassKnot": ("BattleAnimationMotion::TargetSinWave", 31),
        "AnimGunkShotParticles": ("BattleAnimationMotion::GunkShotParticleWave", 31),
        "AnimPsychoCut": ("BattleAnimationMotion::PsychoCutStrike", 31),
        "AnimStealthRock": ("BattleAnimationMotion::StealthRockArc", 64),
        "AnimSuckerPunch": ("BattleAnimationMotion::TargetSinWave", 31),
        "AnimMoveWorrySeed": ("BattleAnimationMotion::WorrySeedArc", 31),
        "AnimLavaPlumeOrbitScatter": ("BattleAnimationMotion::LavaPlumeDrift", 20),
        "AnimPluck": ("BattleAnimationMotion::PluckParticleJump", 32),
        "AnimMoveTrumpCardParticle": ("BattleAnimationMotion::TrumpCardParticleScatter", 32),
        "AnimRockPolishStreak": ("BattleAnimationMotion::RockPolishStreak", 27),
    }
    explicit_state_machine = state_machine_callbacks.get(callback)
    if explicit_state_machine is not None:
        return explicit_state_machine
    # Every genuinely stationary callback is named here after source review.
    # The orientation variants reproduce SetAnimSpriteInitialXOffset (attack
    # direction) and callbacks such as AngerMark (selected battler side).
    # A new callback may no longer become Static simply because no movement
    # keyword happened to match its name.
    reviewed_stationary_callbacks = {
        "AnimHitSplatBasic": ("BattleAnimationMotion::OrientedStatic", 9),
        "AnimLightning": ("BattleAnimationMotion::OrientedStatic", 28),
        "AnimSpriteOnMonPos": ("BattleAnimationMotion::OrientedStatic", 20),
        "AnimLeer": ("BattleAnimationMotion::OrientedStatic", 15),
        "AnimSpinningSparkle": ("BattleAnimationMotion::OrientedStatic", 15),
        "AnimDragonRageFirePlume": ("BattleAnimationMotion::OrientedStatic", 25),
        "AnimAngerMark": ("BattleAnimationMotion::AnchorOrientedStatic", 16),
        "AnimWallSparkle": ("BattleAnimationMotion::OrientedStatic", 20),
        "AnimKinesisZapEnergy": ("BattleAnimationMotion::OrientedStatic", 42),
        "AnimEyeSparkle": ("BattleAnimationMotion::OrientedStatic", 20),
        "AnimWaveFromCenterOfTarget": ("BattleAnimationMotion::OrientedStatic", 35),
        "AnimBlendThinRing": ("BattleAnimationMotion::OrientedStatic", 31),
        "AnimCrossImpact": ("BattleAnimationMotion::OrientedStatic", 64),
        "AnimUproarRing": ("BattleAnimationMotion::OrientedStatic", 31),
        "AnimElectricity": ("BattleAnimationMotion::OrientedStatic", 64),
        "AnimFurySwipes": ("BattleAnimationMotion::Static", 16),
        "AnimFang": ("BattleAnimationMotion::Static", 32),
        "AnimSuperFang": ("BattleAnimationMotion::Static", 8),
        "AnimClawSlash": ("BattleAnimationMotion::Static", 20),
        "AnimHitSplatOnMonEdge": ("BattleAnimationMotion::Static", 9),
        "AnimGrowingChargeOrb": ("BattleAnimationMotion::Static", 172),
        "AnimElectricPuff": ("BattleAnimationMotion::Static", 12),
        "AnimGrowingShockWaveOrb": ("BattleAnimationMotion::Static", 172),
        # These are instantiated and positioned by their owning exact visual
        # tasks; their direct callback only advances a cel/timer or frees it.
        "AnimElectricBoltSegment": ("BattleAnimationMotion::Static", 15),
        "SpriteCB_SmokescreenImpact": ("BattleAnimationMotion::Static", 12),
        "AnimGlareEyeDot": ("BattleAnimationMotion::Static", 37),
        "SpriteCallbackDummy": ("BattleAnimationMotion::Static", 255),
        "AnimSpeedDust": ("BattleAnimationMotion::Static", 15),
        "AnimEruptionLaunchRock": ("BattleAnimationMotion::Static", 112),
        "AnimVoltTackleBolt": ("BattleAnimationMotion::Static", 13),
        "AnimShockWaveProgressingBolt": ("BattleAnimationMotion::Static", 13),
        "AnimMoveFeintSwipe": ("BattleAnimationMotion::SliceArc", 31),
    }
    reviewed_stationary = reviewed_stationary_callbacks.get(callback)
    if reviewed_stationary is not None:
        return reviewed_stationary
    audited_generic = AUDITED_GENERIC_SPRITE_CALLBACKS.get(callback)
    if audited_generic is not None:
        return audited_generic
    # These four callbacks encode visibly different parts of FireRed's ice
    # attacks. Keep them explicit before the broad translation heuristics;
    # reducing all four to Projectile is what made Ice Beam and Blizzard look
    # like the same move on hardware.
    if callback == "AnimIceBeamParticle":
        return "BattleAnimationMotion::IceBeamParticle", 20
    if callback == "AnimSwirlingSnowball":
        return "BattleAnimationMotion::SwirlingSnowball", 20
    if callback == "AnimMoveParticleBeyondTarget":
        return "BattleAnimationMotion::WavyBeyondTarget", 18
    if callback == "AnimIceEffectParticle":
        return "BattleAnimationMotion::IceImpact", 20
    if callback == "AnimBowMon":
        return "BattleAnimationMotion::BattlerBow", 20
    if callback in {"AnimShakeMonOrBattleTerrain", "AnimShakeMonOrBattlePlatforms"}:
        return "BattleAnimationMotion::BattlerShake", 24
    if callback in {"AnimSimplePaletteBlend", "AnimComplexPaletteBlend"}:
        return "BattleAnimationMotion::PalettePulse", 24
    if not tag:
        if "lunge" in key:
            return "BattleAnimationMotion::BattlerLunge", 12
        if "dip" in key or "jump" in key:
            return "BattleAnimationMotion::BattlerDip", 12
        if any(word in key for word in ("slide", "translate", "sway", "rotate", "scale")):
            return "BattleAnimationMotion::BattlerSlide", 16
        if any(word in key for word in ("blend", "palette", "flash", "invert", "grayscale")):
            return "BattleAnimationMotion::PalettePulse", 16
        return "BattleAnimationMotion::ProceduralPulse", 14
    if any(word in key for word in (
            "initanimarctranslation", "translateanimhorizontalarc",
            "translateanimverticalarc", "animtranslatehorizontalarc")):
        return "BattleAnimationMotion::Arc", 18
    if "sin(" in key and "cos(" in key:
        return "BattleAnimationMotion::Orbit", 20
    if any(word in key for word in (
            "initanimlineartranslation", "startanimlineartranslation",
            "translateanimlinear", "animtranslatelinear", "initanimfastlineartranslation")):
        return "BattleAnimationMotion::Projectile", 18
    if any(word in key for word in ("spiral", "orbit", "vortex", "circle")):
        return "BattleAnimationMotion::Orbit", 20
    if any(word in key for word in ("fall", "drop", "rain", "hail")):
        return "BattleAnimationMotion::Falling", 18
    if any(word in key for word in ("rise", "rising", "float", "upward")):
        return "BattleAnimationMotion::Rising", 18
    if any(word in key for word in ("arc", "bounce", "parabola", "leap")):
        return "BattleAnimationMotion::Arc", 18
    if any(word in key for word in ("wave", "sine", "wavy", "flutter")):
        return "BattleAnimationMotion::Wave", 18
    if "sin(" in key or "cos(" in key:
        return "BattleAnimationMotion::Wave", 18
    if any(word in key for word in ("scatter", "burst", "random", "particle", "fragment")):
        return "BattleAnimationMotion::Scatter", 14
    if any(word in key for word in (
            "projectile", "beam", "bolt", "bullet", "beyondtarget", "translate",
            "travel", "whirlwindline", "razorleaf", "needle", "orb", "ball")):
        return "BattleAnimationMotion::Projectile", 18
    return "BattleAnimationMotion::Static", 12


def sprite_travel(template: str, callback: str, motion: str) -> str:
    """Recover visual travel independently from the createsprite anchor.

    FireRed callbacks normally initialize sprite x/y on one battler and put
    the other battler in data[2]/data[4] before starting a translation.  The
    script's ANIM_ATTACKER/ANIM_TARGET argument only owns the sprite and is not
    reliable as its origin.  Keep target-local and attacker-local particles at
    their anchor, and use explicit endpoints only when the callback proves a
    cross-battler translation.
    """
    exact_cross_battler_callbacks = {
        "TranslateAnimSpriteToTargetMonLocation",
        "AnimThrowMistBall",
        "AnimRockBlastRock",
        "AnimDragonFireToTarget",
        "AnimSkyAttackBird",
        # Source-reviewed multi-phase translations.  Their retained motion
        # profiles reproduce the non-linear part, but they still travel from
        # the attacker to the target and must not be reclassified as local
        # merely because they no longer use the generic Projectile enum.
        "AnimToTargetInSinWave",
        "AnimHyperBeamOrb",
        "AnimConfuseRayBallBounce",
        "AnimBulletSeed",
        "AnimTranslateWebThread",
        "AnimWillOWispOrb",
        "AnimRedHeartProjectile",
        "AnimSolarBeamSmallOrb",
        "AnimZapCannonSpark",
        "InitPoisonGasCloudAnim",
    }
    if callback in exact_cross_battler_callbacks:
        return "BattleAnimationTravel::AttackerToTarget"

    if motion not in (
            "BattleAnimationMotion::Projectile",
            "BattleAnimationMotion::StandardLinearProjectile",
            "BattleAnimationMotion::StandardArcProjectile",
            "BattleAnimationMotion::AbsorptionArc",
            "BattleAnimationMotion::AcidBubbleArc",
            "BattleAnimationMotion::Bonemerang",
            "BattleAnimationMotion::CoinThrow",
            "BattleAnimationMotion::OffscreenFly",
            "BattleAnimationMotion::HyperVoiceRing",
            "BattleAnimationMotion::LeechSeedArc",
            "BattleAnimationMotion::MimicOrb",
            "BattleAnimationMotion::AbsorptionLinear",
            "BattleAnimationMotion::SludgeArc",
            "BattleAnimationMotion::SolarBeamBigOrb",
            "BattleAnimationMotion::SpikesArc",
            "BattleAnimationMotion::SunlightRay",
            "BattleAnimationMotion::SuperpowerFireball",
            "BattleAnimationMotion::TriAttackTriangle",
            "BattleAnimationMotion::TargetLocationProjectile",
            "BattleAnimationMotion::DragonFire",
            "BattleAnimationMotion::SkyAttackBird",
            "BattleAnimationMotion::Arc",
            "BattleAnimationMotion::Wave",
            "BattleAnimationMotion::OrbitingProjectile",
            "BattleAnimationMotion::IceBeamParticle",
            "BattleAnimationMotion::SwirlingSnowball",
            "BattleAnimationMotion::WavyBeyondTarget"):
        return "BattleAnimationTravel::Local"

    body = animation_function_bodies.get(callback, "").lower()
    key = f"{template} {callback}".lower()

    starts_attacker = bool(re.search(
        r"initspritepostoanimattacker|sprite->(?:x|y)\s*=\s*"
        r"getbattlerspritecoord2?\(gbattleanimattacker", body))
    starts_target = bool(re.search(
        r"initspritepostoanimtarget|sprite->(?:x|y)\s*=\s*"
        r"getbattlerspritecoord2?\(gbattleanimtarget", body))
    ends_attacker = bool(re.search(
        r"sprite->data\[(?:2|4)\]\s*=\s*getbattlerspritecoord2?\("
        r"gbattleanimattacker|setaveragebattlerpositions\(gbattleanimattacker"
        r"[^;]*&sprite->data\[(?:2|4)\]", body))
    ends_target = bool(re.search(
        r"sprite->data\[(?:2|4)\]\s*=\s*getbattlerspritecoord2?\("
        r"gbattleanimtarget|setaveragebattlerpositions\(gbattleanimtarget"
        r"[^;]*&sprite->data\[(?:2|4)\]", body))

    if starts_target and ends_attacker and not ends_target:
        return "BattleAnimationTravel::TargetToAttacker"
    if starts_attacker and ends_target and not ends_attacker:
        return "BattleAnimationTravel::AttackerToTarget"

    # Multi-stage callbacks can contain both sides in branches.  Their names
    # preserve the reverse direction documented by the original engine.
    if any(word in key for word in (
            "absorption", "mimicorb", "itemsteal", "drainorb")):
        return "BattleAnimationTravel::TargetToAttacker"

    # A callback mentioning only one battler is a local orbit/scatter/offset,
    # even if the coarse motion classifier called it a projectile.
    mentions_attacker = "gbattleanimattacker" in body
    mentions_target = "gbattleanimtarget" in body
    if mentions_attacker != mentions_target:
        return "BattleAnimationTravel::Local"

    if any(word in key for word in (
            "totarget", "projectile", "beam", "bullet", "throw", "missile",
            "webthread", "stinger", "needle", "hydropump", "flamethrower")):
        return "BattleAnimationTravel::AttackerToTarget"

    # Generic translated sprites are overwhelmingly outbound attacks.  This
    # default is deliberately independent from createsprite's allocation owner.
    return "BattleAnimationTravel::AttackerToTarget"


def sprite_motion_coverage(callback: str, motion: str,
                           state_moves_coordinates: bool) -> str:
    """Describe why this callback has a renderer; never equate fallback with fidelity."""
    motion_name = motion.split("::")[-1]
    generic = {"Static", "Projectile", "Arc", "Falling", "Rising",
               "Orbit", "Wave", "Scatter"}
    if motion_name not in generic:
        return "ExactCallbackProfile"
    if callback in AUDITED_GENERIC_SPRITE_CALLBACKS:
        return "ExplicitReviewedCallback"
    body = expanded_callback_body(callback)
    if motion_name == "Static" and not state_moves_coordinates:
        return ("ExplicitReviewedCallback" if callback in REVIEWED_STATIC_CALLBACKS
                else "HeuristicFallback")
    if motion_name in {"Projectile", "Arc"} and re.search(
            r"LinearTranslation|HorizontalArc|VerticalArc|"
            r"TranslateSpriteLinear", body, re.I):
        return "FireRedStandardHelper"
    if motion_name in {"Wave", "Orbit"} and re.search(r"\b(?:Sin|Cos)\s*\(", body):
        return "SourceTrigonometry"
    if motion_name in {"Falling", "Rising"} and re.search(
            r"sprite->(?:y|y2)\s*[+\-]?=", body):
        return "SourceVerticalMotion"
    if motion_name == "Scatter" and re.search(
            r"Random|sprite->(?:x|y|x2|y2)\s*[+\-]?=", body, re.I):
        return "SourceParticleMotion"
    return "HeuristicFallback"


def sprite_origin(template: str, callback: str, travel: str) -> str:
    """Recover the coordinate origin independently from createsprite flags.

    FireRed's Cmd_createsprite always calls CreateSpriteAndAnimate at the
    target coordinates.  ANIM_ATTACKER/ANIM_TARGET in the script controls only
    OBJ subpriority.  Callbacks may subsequently relocate that OBJ to the
    attacker, or select a battler through a gBattleAnimArgs slot.  Treating the
    script flag as an anchor put Bite/Crunch and many hit effects on the user.
    """
    body = animation_function_bodies.get(callback, "")

    explicit_selector_origins = {
        "AnimMovementWaves": "BattleAnimationOrigin::Argument0",
        "AnimDigDirtMound": "BattleAnimationOrigin::Argument0",
        "AnimHitSplatHandleInvert": "BattleAnimationOrigin::Argument2",
        "AnimHitSplatPersistent": "BattleAnimationOrigin::Argument2",
        "AnimBasicFistOrFoot": "BattleAnimationOrigin::Argument3",
        "AnimDiveWaterSplash": "BattleAnimationOrigin::Argument0",
        "AnimTailGlowOrb": "BattleAnimationOrigin::Argument0",
        "AnimRevengeScratch": "BattleAnimationOrigin::Argument2",
    }
    if callback in explicit_selector_origins:
        return explicit_selector_origins[callback]

    # Pain Split's third argument explicitly chooses which of the two
    # battlers owns each spark.  Both variants occur in every use of the move.
    if callback == "AnimPainSplitProjectile":
        return "BattleAnimationOrigin::Argument2"

    # These callbacks first resolve an argument into a local ``battler`` and
    # then pass that variable to SetSpriteNextToMonHead.  The generic textual
    # matcher cannot see through the local variable, but arg0 retains the
    # normal ANIM_ATTACKER/ANIM_TARGET contract in all three callbacks.
    if callback in {"AnimMetronomeFinger", "AnimTauntFinger",
                    "AnimThoughtBubble"}:
        return "BattleAnimationOrigin::Argument0"

    if callback in {"AnimBowMon", "AnimShakeMonOrBattleTerrain",
                    "AnimSimplePaletteBlend", "AnimComplexPaletteBlend"}:
        return "BattleAnimationOrigin::Attacker"

    # Generic callbacks choose 0/ANIM_ATTACKER versus nonzero/ANIM_TARGET.
    # Locate the nearest qualifying if before InitSpritePosToAnimAttacker so a
    # preceding unrelated condition (AnimSpriteOnMonPos uses arg3 for picture
    # offsets) cannot be mistaken for the battler selector.
    selector_condition = re.compile(
        r"if\s*\(\s*(?:!\s*gBattleAnimArgs\[(\d)\]|"
        r"gBattleAnimArgs\[(\d)\]\s*(?:==|!=)\s*(?:0|ANIM_ATTACKER)|"
        r"(?:0|ANIM_ATTACKER)\s*(?:==|!=)\s*gBattleAnimArgs\[(\d)\])\s*\)")

    # Some callbacks choose the battler and then assign coordinates directly
    # with GetBattlerSpriteCoord instead of the InitSpritePos* helpers.  The
    # old inference missed that family and fell back to Target.  Focus Energy's
    # AnimEndureEnergy is the smallest visible example: argument 0 is zero in
    # its script, so every energy particle belongs to the attacker.
    #
    lowered = body.lower()
    attacker_markers = (
        "initspritepostoanimattacker",
        "setspritecoordstoanimattackercoords",
        "getbattlerspritecoord(gbattleanimattacker",
        "getbattlerspritecoord2(gbattleanimattacker",
        "setaveragebattlerpositions(gbattleanimattacker",
    )
    target_markers = (
        "initspritepostoanimtarget",
        "getbattlerspritecoord(gbattleanimtarget",
        "getbattlerspritecoord2(gbattleanimtarget",
        "setaveragebattlerpositions(gbattleanimtarget",
    )
    explicitly_attacker = any(marker in lowered for marker in attacker_markers)
    explicitly_target = any(marker in lowered for marker in target_markers)

    for attacker_init in re.finditer(r"InitSpritePosToAnimAttacker", body):
        preceding = list(selector_condition.finditer(body[:attacker_init.start()]))
        if not preceding:
            continue
        selector = preceding[-1]
        following = body[attacker_init.end():attacker_init.end() + 240]
        if "else" in following and "InitSpritePosToAnimTarget" in following:
            argument = next(
                int(group) for group in selector.groups() if group is not None)
            return f"BattleAnimationOrigin::Argument{argument}"

    # Preserve the argument itself as the origin only when both branches
    # actually assign battler coordinates. Merely mentioning the attacker for
    # side-dependent X mirroring is not a battler selector: AnimBubbleEffect,
    # for example, uses arg 2 for single/multi-target mode and always starts on
    # gBattleAnimTarget. Treating that flag as ANIM_ATTACKER put Poison Sting's
    # final bubbles on the Pokemon that fired the needle. This direct-coordinate
    # fallback runs after the InitSpritePos pair above so an unrelated earlier
    # condition cannot steal its selector (as happened with Scratch).
    selector = selector_condition.search(body)
    if selector and explicitly_attacker and explicitly_target:
        argument = next(int(group) for group in selector.groups() if group is not None)
        return f"BattleAnimationOrigin::Argument{argument}"

    # Two common helpers select a battler without calling InitSpritePos*.  The
    # arguments still use the same ANIM_ATTACKER=0 / ANIM_TARGET=1 convention.
    direct_argument = re.search(
        r"GetAnimBattlerSpriteId\(gBattleAnimArgs\[(\d)\]\)", body)
    if direct_argument:
        return f"BattleAnimationOrigin::Argument{direct_argument.group(1)}"
    if callback == "AnimFistOrFootRandomPos":
        return "BattleAnimationOrigin::Argument0"

    # Travel supplies its own source and destination, so its allocation origin
    # is irrelevant.  Keeping Target mirrors CreateSpriteAndAnimate exactly.
    if travel != "BattleAnimationTravel::Local":
        return "BattleAnimationOrigin::Target"
    if explicitly_attacker and not explicitly_target:
        return "BattleAnimationOrigin::Attacker"
    # Target is both the explicit choice and the hardware-faithful default.
    return "BattleAnimationOrigin::Target"


# These callbacks exercise every important distinction.  Keep this audit in
# the generator so a later heuristic edit cannot silently reverse the battle
# again while still producing compilable data.
travel_audit = {
    "gGoldRingSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gAuroraBeamRingSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gIceBeamOuterCrystalSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gFlamethrowerFlameSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gWaterGunProjectileSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gWaterBubbleProjectileSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gAbsorptionOrbSpriteTemplate": "BattleAnimationTravel::TargetToAttacker",
    "gMimicOrbSpriteTemplate": "BattleAnimationTravel::TargetToAttacker",
    "gLeechLifeNeedleSpriteTemplate": "BattleAnimationTravel::Local",
    "gReversalOrbSpriteTemplate": "BattleAnimationTravel::Local",
    "gDragonBreathFireSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gDragonRageFireSpitSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gMistBallSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gRockBlastRockSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
    "gSkyAttackBirdSpriteTemplate": "BattleAnimationTravel::AttackerToTarget",
}
for audited_template, expected_travel in travel_audit.items():
    audited_spec = template_specs.get(audited_template)
    if audited_spec is None:
        raise RuntimeError(f"Missing audited animation template: {audited_template}")
    audited_motion, _ = sprite_motion(
        audited_template, audited_spec["callback"], audited_spec["tag"])
    actual_travel = sprite_travel(
        audited_template, audited_spec["callback"], audited_motion)
    if actual_travel != expected_travel:
        raise RuntimeError(
            f"Animation direction audit failed for {audited_template}: "
            f"expected {expected_travel}, got {actual_travel}")

# These cover target-default, explicit-attacker and argument-selected local
# sprites. They make the original Bite regression a generator failure rather
# than something the player has to rediscover on hardware.
origin_audit = {
    "gSharpTeethSpriteTemplate": "BattleAnimationOrigin::Target",
    "gClampJawSpriteTemplate": "BattleAnimationOrigin::Target",
    "gSwordsDanceBladeSpriteTemplate": "BattleAnimationOrigin::Attacker",
    "gSpinningSparkleSpriteTemplate": "BattleAnimationOrigin::Attacker",
    "gScratchSpriteTemplate": "BattleAnimationOrigin::Argument2",
    "gFistFootSpriteTemplate": "BattleAnimationOrigin::Argument3",
    "gRandomPosHitSplatSpriteTemplate": "BattleAnimationOrigin::Argument0",
    "gBasicHitSplatSpriteTemplate": "BattleAnimationOrigin::Argument2",
    # AnimBubbleEffect's arg 2 is the single/multi-target mode, not an
    # ANIM_ATTACKER/ANIM_TARGET selector. Both poison and water variants must
    # remain attached to the move recipient.
    "gPoisonBubbleSpriteTemplate": "BattleAnimationOrigin::Target",
    "gWaterBubbleSpriteTemplate": "BattleAnimationOrigin::Target",
    # Direct GetBattlerSpriteCoord branches must retain their selector.  This
    # guards every Focus Energy / Endure particle against being moved to the
    # opposing Pokemon by a later generator refactor.
    "gEndureEnergySpriteTemplate": "BattleAnimationOrigin::Argument0",
    "gPainSplitProjectileSpriteTemplate": "BattleAnimationOrigin::Argument2",
}
for audited_template, expected_origin in origin_audit.items():
    audited_spec = template_specs.get(audited_template)
    if audited_spec is None:
        raise RuntimeError(f"Missing audited animation origin template: {audited_template}")
    audited_motion, _ = sprite_motion(
        audited_template, audited_spec["callback"], audited_spec["tag"])
    audited_travel = sprite_travel(
        audited_template, audited_spec["callback"], audited_motion)
    actual_origin = sprite_origin(
        audited_template, audited_spec["callback"], audited_travel)
    if actual_origin != expected_origin:
        raise RuntimeError(
            f"Animation origin audit failed for {audited_template}: "
            f"expected {expected_origin}, got {actual_origin}")

ice_motion_audit = {
    "gIceBeamOuterCrystalSpriteTemplate":
        ("BattleAnimationMotion::IceBeamParticle", "ice_beam_outer_crystal"),
    "gIceBeamInnerCrystalSpriteTemplate":
        ("BattleAnimationMotion::IceBeamParticle", "ice_beam_inner_crystal"),
    "gSwirlingSnowballSpriteTemplate":
        ("BattleAnimationMotion::SwirlingSnowball", "blizzard_snowball"),
    "gBlizzardIceCrystalSpriteTemplate":
        ("BattleAnimationMotion::WavyBeyondTarget", "blizzard_ice_crystal"),
    "gIceCrystalHitLargeSpriteTemplate":
        ("BattleAnimationMotion::IceImpact", "ice_crystal_hit_large"),
    "gIceCrystalHitSmallSpriteTemplate":
        ("BattleAnimationMotion::IceImpact", "ice_crystal_hit_small"),
}
for audited_template, (expected_motion, expected_asset) in ice_motion_audit.items():
    audited_spec = template_specs.get(audited_template)
    if audited_spec is None:
        raise RuntimeError(f"Missing audited ice template: {audited_template}")
    audited_motion, _ = sprite_motion(
        audited_template, audited_spec["callback"], audited_spec["tag"])
    audited_asset = template_visual_asset_spec(
        audited_template, audited_spec["tag"])
    if audited_motion != expected_motion or not audited_asset or \
            audited_asset["key"] != expected_asset:
        raise RuntimeError(
            f"Ice animation audit failed for {audited_template}: "
            f"motion={audited_motion}, asset={audited_asset and audited_asset['key']}")

# Lock the callback-specific positional families too. This turns a future
# accidental reintroduction of the generic Orbit classifier into a generator
# failure instead of another hardware-only centering regression.
positional_motion_audit = {
    "gFireSpinSpriteTemplate": "BattleAnimationMotion::Vortex",
    "gWhirlpoolSpriteTemplate": "BattleAnimationMotion::Vortex",
    "gSwirlingDirtSpriteTemplate": "BattleAnimationMotion::Vortex",
    "gFireSpiralInwardSpriteTemplate": "BattleAnimationMotion::GrowingCircle",
    "gIceCrystalSpiralInwardSmall": "BattleAnimationMotion::GrowingCircle",
    "gIceCrystalSpiralInwardLarge": "BattleAnimationMotion::GrowingCircle",
    "gRazorWindTornadoSpriteTemplate": "BattleAnimationMotion::Circular",
    "gWaterBubbleProjectileSpriteTemplate": "BattleAnimationMotion::OrbitingProjectile",
    "gConfuseRayBallSpiralSpriteTemplate": "BattleAnimationMotion::ConfuseRaySpiral",
    "gHiddenPowerOrbSpriteTemplate": "BattleAnimationMotion::FastOrbit",
    "gHiddenPowerOrbScatterSpriteTemplate": "BattleAnimationMotion::RadialScatter",
    "gFireSpiralOutwardSpriteTemplate": "BattleAnimationMotion::ExpandingSpiral",
    "gWillOWispFireSpriteTemplate": "BattleAnimationMotion::ExpandingEllipse",
    "gOverheatFlameSpriteTemplate": "BattleAnimationMotion::RadialLinear",
    "gDragonDanceOrbSpriteTemplate": "BattleAnimationMotion::DragonDanceOrbit",
    "gPsychUpSpiralSpriteTemplate": "BattleAnimationMotion::OrientedStatic",
    "gLusterPurgeCircleSpriteTemplate": "BattleAnimationMotion::OrientedStatic",
}
for audited_template, expected_motion in positional_motion_audit.items():
    audited_spec = template_specs.get(audited_template)
    if audited_spec is None:
        raise RuntimeError(f"Missing audited positional template: {audited_template}")
    actual_motion, _ = sprite_motion(
        audited_template, audited_spec["callback"], audited_spec["tag"])
    if actual_motion != expected_motion:
        raise RuntimeError(
            f"Animation positional audit failed for {audited_template}: "
            f"expected {expected_motion}, got {actual_motion}")


QUERY_TASKS = {
    "AnimTask_IsContest": "BattleAnimationQuery::IsContest",
    "AnimTask_GetAttackerSide": "BattleAnimationQuery::AttackerSide",
    "AnimTask_GetTargetSide": "BattleAnimationQuery::TargetSide",
    "AnimTask_IsTargetPlayerSide": "BattleAnimationQuery::IsTargetPlayerSide",
    "AnimTask_GetTargetIsAttackerPartner": "BattleAnimationQuery::IsTargetAttackerPartner",
    "AnimTask_IsFuryCutterHitRight": "BattleAnimationQuery::IsFuryCutterHitRight",
    "AnimTask_GetFuryCutterHitCount": "BattleAnimationQuery::FuryCutterHitCount",
    "AnimTask_GetFrustrationPowerLevel": "BattleAnimationQuery::FrustrationPowerLevel",
    "AnimTask_GetReturnPowerLevel": "BattleAnimationQuery::ReturnPowerLevel",
    "AnimTask_IsPowerOver99": "BattleAnimationQuery::IsMovePowerOver99",
    "AnimTask_IsHealingMove": "BattleAnimationQuery::IsHealingMove",
    "AnimTask_GetSeismicTossDamageLevel": "BattleAnimationQuery::SeismicTossDamageLevel",
    "AnimTask_GetBattleTerrain": "BattleAnimationQuery::BattleTerrain",
    "AnimTask_GetRolloutCounter": "BattleAnimationQuery::RolloutCounter",
    "AnimTask_GetWeather": "BattleAnimationQuery::Weather",
    "GetIsDoomDesireHitTurn": "BattleAnimationQuery::IsDoomDesireHitTurn",
    "AnimTask_IsMonInvisible": "BattleAnimationQuery::IsMonInvisible",
}


def task_kind(name: str) -> str:
    lowered = name.lower()
    body = expanded_task_callback_body(name).lower()
    if name in {"AnimTask_AllySwitchAttacker", "AnimTask_AllySwitchPartner",
                "AnimTask_SnatchOpposingMonMove"}:
        return "BattleAnimationTaskKind::BattlerMotion"
    if name == "AnimTask_TechnoBlast":
        return "BattleAnimationTaskKind::Control"
    if lowered.startswith("soundtask_"):
        return "BattleAnimationTaskKind::Sound"
    if (lowered.startswith("animtask_get") or
            lowered.startswith("animtask_is") or
            lowered.startswith("getis") or
            any(word in lowered for word in (
                "allocbackuppal", "freebackuppal", "loadmusicnotespals",
                "freemusicnotespals", "startsinanimtimer", "setanimtarget"))):
        return "BattleAnimationTaskKind::Control"
    # Singles renderer: this task only adjusts BG priority in a double battle.
    if "sporedoublebattle" in lowered:
        return "BattleAnimationTaskKind::Control"
    # Role Play clones/blends a battler silhouette; it never emits an
    # independent particle. Treat it as battler presentation, not a pulse.
    if "roleplaysilhouette" in lowered:
        return "BattleAnimationTaskKind::BattlerMotion"
    if "shake" in lowered:
        return "BattleAnimationTaskKind::Shake"
    if any(word in lowered for word in (
            "translate", "scale", "squish", "sway", "rotate", "slide", "lunge",
            "sink", "wobble", "elliptical", "movement", "stretch", "shrink",
            "grow", "deform", "elevation", "teleport", "withdraw", "doubleteam",
            "transformmon", "clone", "disappear", "reappear", "rockmon",
            "rapidspinmon", "thrashmovemon", "acidarmor", "minimize",
            "substitute", "skullbashposition", "deepinhale")):
        return "BattleAnimationTaskKind::BattlerMotion"
    if any(word in lowered for word in (
            "blend", "palette", "color", "flash", "invert", "grayscale", "fade")):
        return "BattleAnimationTaskKind::Palette"
    procedural_backgrounds = {
        "animtask_setpsychicbackground": "PsychicBackground",
        "animtask_startslidingbg": "SlidingBackground",
        "animtask_loadsandstormbackground": "SandstormBackground",
        "animtask_createspotlight": "SpotlightBackground",
        "animtask_removespotlight": "SpotlightBackground",
        "animtask_extrasensorydistortion": "DistortionBackground",
        "animtask_dragondancewaver": "DistortionBackground",
        "animtask_moveheatwavetargets": "DistortionBackground",
        "animtask_sketchdrawmon": "DistortionBackground",
        "animtask_moveattackermementoshadow": "DistortionBackground",
        "animtask_movetargetmementoshadow": "DistortionBackground",
        "animtask_mistballfog": "FogBackground",
        "animtask_hazescrollingfog": "FogBackground",
        "animtask_morningsunlightbeam": "SunlightBackground",
        "animtask_heartsbackground": "HeartBackground",
        "animtask_moveskyuppercutbg": "CinematicBackground",
        "animtask_positionfissurebgonbattler": "CinematicBackground",
        "animtask_moveseismictossbg": "CinematicBackground",
        "animtask_seismictossbgacceleratedownatend": "CinematicBackground",
    }
    procedural_kind = procedural_backgrounds.get(lowered)
    if procedural_kind:
        return f"BattleAnimationTaskKind::{procedural_kind}"
    if any(word in lowered for word in (
            "background", "slidingbg", "terrain", "weather", "rain", "hail",
            "sandstorm", "sunlight", "fog", "haze", "surfwave", "spotlight",
            "fissurebg", "skyuppercutbg", "seismictossbg", "heatwavetargets")):
        return "BattleAnimationTaskKind::Background"
    # Names in the original source are not consistently descriptive. Inspect
    # the complete installed task-state graph before assigning the old generic
    # Effect renderer: a large share of these callbacks merely deform/hide a
    # battler, cycle its palette, or operate a scanline/background mask.
    if any(word in body for word in (
            "blendpalette", "fillpalette", "loadpalette", "palettefade",
            "bldalpha", "bldy", "bldcnt")) and "createsprite" not in body:
        return "BattleAnimationTaskKind::Palette"
    if any(word in body for word in (
            "scanlineeffect", "animloadcompressedbg", "battleanimbgtilemap",
            "battleanimgfx", "scrollingbgmask", "reg_offset_bg",
            "reg_offset_win")):
        return "BattleAnimationTaskKind::Background"
    if any(word in body for word in (
            "preparespriteforrot", "prepareaffineanimintaskdata",
            "runaffineanimfromtaskdata", ".x2", ".y2", "->x2", "->y2",
            ".invisible", "->invisible", "battlerspriteids")) and \
            "createsprite" not in body:
        return "BattleAnimationTaskKind::BattlerMotion"
    return "BattleAnimationTaskKind::Effect"


TASK_INTERNAL_SPRITE_TEMPLATES: dict[str, tuple[str, ...]] = {
    # Bestow creates the held-item icon dynamically. The retained renderer
    # uses the canonical item-bag cel from Present with the same authored
    # projectile arguments, avoiding a runtime dependency on item icon VRAM.
    "AnimTask_CreateBestowItem": ("gPresentSpriteTemplate",),
}


def task_sprite_templates(name: str) -> list[str]:
    """Exact SpriteTemplates instantiated inside a visual task callback."""
    body = expanded_task_callback_body(name)
    templates = {
        template for template in re.findall(
            r"\b([A-Za-z_][A-Za-z0-9_]*SpriteTemplate)\b", body)
        if template in template_specs
    }
    templates.update(TASK_INTERNAL_SPRITE_TEMPLATES.get(name, ()))
    return sorted(templates)


# Explicit retained-renderer semantics for every FireRed task callback that
# manipulates a battler or its palette.  These tables are intentionally not
# fuzzy name matching: adding a new callback without deciding what it does is
# a generation error.  This is the per-routine audit that prevents all
# deformation tasks from regressing to one generic wobble.
TASK_MOTION_RESOURCES: dict[str, tuple[str, str]] = {
    "AnimTask_AllySwitchAttacker": ("DoubleTeam", "Attacker"),
    "AnimTask_AllySwitchPartner": ("DoubleTeam", "Target"),
    "AnimTask_AcidArmor": ("AcidArmor", "ScriptAnchor"),
    "AnimTask_AttackerStretchAndDisappear": ("AttackerStretchAndDisappear", "Attacker"),
    "AnimTask_CurseStretchingBlackBg": ("CurseStretchingBlackBg", "Screen"),
    "AnimTask_DeepInhale": ("DeepInhale", "Attacker"),
    "AnimTask_CompressTargetHorizontally": ("CompressTargetHorizontally", "Target"),
    "AnimTask_CompressTargetHorizontallyFast": ("CompressTargetHorizontallyFast", "Target"),
    "AnimTask_DefenseCurlDeformMon": ("DefenseCurlDeformMon", "Attacker"),
    "AnimTask_DigDownMovement": ("DigDownMovement", "Attacker"),
    "AnimTask_DigUpMovement": ("DigUpMovement", "Attacker"),
    "AnimTask_DoubleTeam": ("DoubleTeam", "Attacker"),
    # The impact callback displaces the struck battler.  The old retained
    # mapping incorrectly moved the attacker and made Extreme Speed appear to
    # recoil away from its own hit.
    "AnimTask_ExtremeSpeedImpact": ("ExtremeSpeedImpact", "Target"),
    "AnimTask_ExtremeSpeedMonReappear": ("ExtremeSpeedMonReappear", "Attacker"),
    "AnimTask_FlailMovement": ("FlailMovement", "Attacker"),
    "AnimTask_GrowAndGrayscale": ("GrowAndGrayscale", "Target"),
    "AnimTask_GrowAndShrink": ("GrowAndShrink", "Attacker"),
    "AnimTask_HelpingHandAttackerMovement": ("HelpingHandAttackerMovement", "Attacker"),
    "AnimTask_MeditateStretchAttacker": ("MeditateStretchAttacker", "Attacker"),
    "AnimTask_Minimize": ("Minimize", "Attacker"),
    "AnimTask_MonToSubstitute": ("MonToSubstitute", "Attacker"),
    # FireRed clones the attacker here; it is not a target after-image.
    "AnimTask_NightShadeClone": ("NightShadeClone", "Attacker"),
    "AnimTask_NightmareClone": ("NightmareClone", "Target"),
    "AnimTask_OdorSleuthMovement": ("OdorSleuthMovement", "Target"),
    "AnimTask_PainSplitMovement": ("PainSplitMovement", "BothBattlers"),
    "AnimTask_RapinSpinMonElevation": ("RapidSpinMonElevation", "Attacker"),
    "AnimTask_RockMonBackAndForth": ("RockMonBackAndForth", "Attacker"),
    "AnimTask_RolePlaySilhouette": ("RolePlaySilhouette", "Target"),
    "AnimTask_RotateAuroraRingColors": ("RotateAuroraRingColors", "None"),
    "AnimTask_RotateMonSpriteToSide": ("RotateMonSpriteToSide", "ScriptAnchor"),
    "AnimTask_RotateMonToSideAndRestore": ("RotateMonToSideAndRestore", "ScriptAnchor"),
    "AnimTask_RotateVertically": ("RotateVerticallyExact", "ScriptAnchor"),
    "AnimTask_ScaleMonAndRestore": ("ScaleMonAndRestore", "ScriptAnchor"),
    "AnimTask_SnatchOpposingMonMove": ("SnatchOpposingMonMoveExact", "Attacker"),
    "AnimTask_SetAllNonAttackersInvisiblity": ("SetAllNonAttackersInvisible", "Target"),
    "AnimTask_SetAttackerInvisibleWaitForSignal": ("SetAttackerInvisibleWaitForSignal", "Attacker"),
    "AnimTask_SetGrayscaleOrOriginalPal": ("SetGrayscaleOrOriginalPal", "ScriptAnchor"),
    "AnimTask_ShrinkTargetCopy": ("ShrinkTargetCopy", "Target"),
    "AnimTask_SkullBashPosition": ("SkullBashPosition", "Attacker"),
    "AnimTask_SlackOffSquish": ("SlackOffSquish", "Attacker"),
    "AnimTask_SlideOffScreen": ("SlideOffScreenExact", "ScriptAnchor"),
    "AnimTask_SmellingSaltsSquish": ("SmellingSaltsSquish", "Target"),
    # The expansion names the same target deformation directly for Gen-IV
    # moves; the retained renderer already has the source-compatible squish
    # profile used by Smelling Salts.
    "AnimTask_SquishTarget": ("SmellingSaltsSquish", "Target"),
    "AnimTask_SpitUpDeformMon": ("SpitUpDeformMon", "Attacker"),
    "AnimTask_Splash": ("Splash", "ScriptAnchor"),
    "AnimTask_SquishAndSweatDroplets": ("SquishAndSweatDroplets", "Attacker"),
    "AnimTask_StockpileDeformMon": ("StockpileDeformMon", "Attacker"),
    "AnimTask_StretchAttackerUp": ("StretchAttackerUp", "Attacker"),
    "AnimTask_StretchTargetUp": ("StretchTargetUp", "Target"),
    "AnimTask_StrongFrustrationGrowAndShrink": ("StrongFrustrationGrowAndShrink", "Attacker"),
    "AnimTask_SwallowDeformMon": ("SwallowDeformMon", "Attacker"),
    "AnimTask_SwayMon": ("SwayMonExact", "ScriptAnchor"),
    "AnimTask_TeeterDanceMovement": ("TeeterDanceMovement", "Attacker"),
    "AnimTask_Teleport": ("TeleportExact", "Attacker"),
    "AnimTask_ThrashMoveMonHorizontal": ("ThrashMoveMonHorizontal", "Attacker"),
    "AnimTask_ThrashMoveMonVertical": ("ThrashMoveMonVertical", "Attacker"),
    "AnimTask_TransformMon": ("TransformMon", "Attacker"),
    "AnimTask_TranslateMonElliptical": ("TranslateMonElliptical", "ScriptAnchor"),
    "AnimTask_TranslateMonEllipticalRespectSide": ("TranslateMonEllipticalRespectSide", "ScriptAnchor"),
    "AnimTask_TransparentCloneGrowAndShrink": ("TransparentCloneGrowAndShrink", "ScriptAnchor"),
    "AnimTask_UproarDistortion": ("UproarDistortion", "Attacker"),
    "AnimTask_VoltTackleAttackerReappear": ("VoltTackleAttackerReappear", "Attacker"),
    "AnimTask_WindUpLunge": ("WindUpLunge", "Attacker"),
    "AnimTask_Withdraw": ("WithdrawExact", "Attacker"),
}

TASK_PALETTE_RESOURCES: dict[str, tuple[str, str]] = {
    "AnimTask_AlphaFadeIn": ("AlphaFadeInExact", "ScriptAnchor"),
    "AnimTask_AnimateGustTornadoPalette": ("TagPaletteRotate", "None"),
    "AnimTask_AttackerFadeFromInvisible": ("AttackerFadeFromInvisibleExact", "Attacker"),
    "AnimTask_AttackerFadeToInvisible": ("AttackerFadeToInvisibleExact", "Attacker"),
    "AnimTask_AttackerPunchWithTrace": ("AttackerPunchWithTraceExact", "Attacker"),
    "AnimTask_BlendBackground": ("BlendBackgroundExact", "Screen"),
    "AnimTask_BlendBattleAnimPal": ("BlendBattleAnimPalExact", "ScriptAnchor"),
    "AnimTask_BlendBattleAnimPalExclude": ("BlendBattleAnimPalExcludeExact", "BothBattlers"),
    "AnimTask_BlendColorCycle": ("BlendColorCycleExact", "ScriptAnchor"),
    "AnimTask_BlendColorCycleByTag": ("TagBlendCycle", "None"),
    "AnimTask_BlendColorCycleExclude": ("BlendColorCycleExcludeExact", "BothBattlers"),
    "AnimTask_BlendMonInAndOut": ("BlendMonInAndOutExact", "ScriptAnchor"),
    "AnimTask_BlendNonAttackerPalettes": ("BlendNonAttackerPalettesExact", "Target"),
    "AnimTask_BlendPalInAndOutByTag": ("TagBlendInOut", "None"),
    "AnimTask_BlendParticle": ("TagBlendToColor", "None"),
    "AnimTask_Conversion2AlphaBlend": ("Conversion2AlphaBlendExact", "Attacker"),
    "AnimTask_ConversionAlphaBlend": ("ConversionAlphaBlendExact", "Attacker"),
    "AnimTask_CopyPalFadedToUnfaded": ("PaletteBackupCommit", "None"),
    "AnimTask_CopyPalUnfadedFromBackup": ("PaletteBackupRestore", "None"),
    "AnimTask_CopyPalUnfadedToBackup": ("PaletteBackupSave", "None"),
    "AnimTask_CycleMagicalLeafPal": ("MagicalLeafPaletteCycle", "None"),
    "AnimTask_DrawFallingWhiteLinesOnAttacker": ("CurseWhiteLines", "Attacker"),
    "AnimTask_FacadeColorBlend": ("FacadeColorBlendExact", "Attacker"),
    "AnimTask_FadeScreenToWhite": ("FadeScreenToWhiteExact", "Screen"),
    "AnimTask_FakeOut": ("FakeOutExact", "Screen"),
    "AnimTask_Flash": ("FlashExact", "ScriptAnchor"),
    "AnimTask_FlashAnimTagWithColor": ("TagFlash", "None"),
    "AnimTask_HardwarePaletteFade": ("HardwarePaletteFadeExact", "Screen"),
    "AnimTask_InitAttackerFadeFromInvisible": ("InitAttackerFadeFromInvisibleExact", "Attacker"),
    "AnimTask_InitMementoShadow": ("InitMementoShadowExact", "Target"),
    "AnimTask_InvertScreenColor": ("InvertScreenColorExact", "Screen"),
    "AnimTask_MetallicShine": ("MetallicShineExact", "ScriptAnchor"),
    "AnimTask_MoonlightEndFade": ("MoonlightEndFadeExact", "Screen"),
    "AnimTask_MusicNotesClearRainbowBlend": ("MusicNotePaletteClear", "None"),
    "AnimTask_MusicNotesRainbowBlend": ("MusicNotePaletteSetup", "None"),
    # The source task blends the loaded ANIM_TAG_SLASH palette directly; it
    # does not receive a palette tag in gBattleAnimArgs like the generic tag
    # helpers do.
    "AnimTask_BlendNightSlash": ("NightSlashPalette", "None"),
    "AnimTask_ScaryFace": ("ScaryFaceExact", "Target"),
    "AnimTask_SetCamouflageBlend": ("SetCamouflageBlendExact", "Attacker"),
    "AnimTask_SpiteTargetShadow": ("SpiteTargetShadowExact", "Target"),
    "AnimTask_StatusClearedEffect": ("StatusClearedEffectExact", "ScriptAnchor"),
    # The first FireRed argument selects the battler being traced. AGILITY
    # passes ANIM_ATTACKER (0); pinning this task to Target made its afterimage
    # flash on the foe instead of the user.
    "AnimTask_TraceMonBlended": ("TraceMonBlendedExact", "ScriptAnchor"),
}

TASK_SHAKE_RESOURCES: dict[str, tuple[str, str]] = {
    "AnimTask_HorizontalShake": ("HorizontalShakeExact", "ScriptAnchor"),
    "AnimTask_ShakeAndSinkMon": ("ShakeAndSinkMonExact", "ScriptAnchor"),
    "AnimTask_ShakeBattleTerrain": ("ShakeBattleTerrainExact", "Screen"),
    # Expansion's macro spelling for the same authored platform callback.
    "AnimTask_ShakeBattlePlatforms": ("ShakeBattleTerrainExact", "Screen"),
    "AnimTask_ShakeMon": ("ShakeMonExact", "ScriptAnchor"),
    "AnimTask_ShakeMon2": ("ShakeMon2Exact", "ScriptAnchor"),
    "AnimTask_ShakeMonInPlace": ("ShakeMonInPlaceExact", "ScriptAnchor"),
    "AnimTask_ShakeTargetBasedOnMovePowerOrDmg": ("ShakeTargetPowerOrDamage", "Target"),
    "AnimTask_ShakeTargetPartnerBasedOnMovePowerOrDmg": ("ShakeTargetPowerOrDamage", "Target"),
    "AnimTask_ShakeTargetInPattern": ("ShakeTargetInPatternExact", "Target"),
}

TASK_EXACT_SPRITE_RESOURCES: dict[str, tuple[str, str]] = {
    "AnimTask_AirCutterProjectile": ("AirCutterProjectiles", "Target"),
    "AnimTask_BarrageBall": ("BarrageBall", "Target"),
    "AnimTask_CreateBestowItem": ("BestowItemThrow", "Target"),
    "AnimTask_BrineRain": ("Raindrops", "Screen"),
    "AnimTask_CreateRaindrops": ("Raindrops", "Screen"),
    "AnimTask_CreateSmallSolarBeamOrbs": ("SolarBeamOrbs", "Attacker"),
    "AnimTask_DestinyBondWhiteShadow": ("DestinyBondShadow", "Attacker"),
    "AnimTask_DrillPeckHitSplats": ("DrillPeckHitSplats", "Target"),
    "AnimTask_ElectricBolt": ("ElectricBoltSegments", "Target"),
    "AnimTask_ElectricChargingParticles": ("ElectricChargingParticles", "Attacker"),
    "AnimTask_EruptionLaunchRocks": ("EruptionLaunchRocks", "Attacker"),
    "AnimTask_FrozenIceCube": ("FrozenIceCube", "Target"),
    "AnimTask_GlareEyeDots": ("GlareEyeDots", "Target"),
    "AnimTask_GrudgeFlames": ("GrudgeFlames", "Attacker"),
    "AnimTask_PurpleFlamesOnTarget": ("GrudgeFlames", "Target"),
    "AnimTask_Hail": ("HailStones", "Screen"),
    "AnimTask_ImprisonOrbs": ("ImprisonOrbs", "Attacker"),
    "AnimTask_CreateIons": ("IonRain", "Screen"),
    "AnimTask_LeafBlade": ("LeafBladePath", "Target"),
    "AnimTask_Rollout": ("RolloutDebris", "Attacker"),
    "AnimTask_ShockWaveLightning": ("ShockWaveLightning", "Target"),
    "AnimTask_ShockWaveProgressingBolt": ("ShockWaveProgressingBolt", "Target"),
    # Heart Swap is explicitly copied from Skill Swap in the expansion; it
    # only swaps the orb palette, so the retained orb-vault renderer is the
    # source-faithful implementation for both.
    "AnimTask_HeartSwap": ("SkillSwapOrbs", "Target"),
    "AnimTask_SkillSwap": ("SkillSwapOrbs", "Target"),
    "AnimTask_SmokescreenImpact": ("SmokescreenImpact", "Target"),
    "AnimTask_SpeedDust": ("SpeedDust", "Attacker"),
    "AnimTask_TormentAttacker": ("TormentThoughtBubbles", "Attacker"),
    "AnimTask_VoltTackleBolt": ("VoltTackleBolts", "Target"),
    "AnimTask_WaterSport": ("WaterSportOrbs", "Attacker"),
    "AnimTask_WaterSpoutLaunch": ("WaterSpoutLaunch", "Attacker"),
    "AnimTask_WaterSpoutRain": ("WaterSpoutRain", "Screen"),
}

# createvisualtask callbacks that own a BG, a GBA window, or a scanline
# deformation. Every row was translated from the corresponding FireRed C
# state machine; none may fall through to a name-based "background scroll".
# Values are motion, target, exact/upper-bound lifetime, blocks waitforvisual.
TASK_BACKGROUND_RESOURCES: dict[str, tuple[str, str, int, bool]] = {
    # Surf owns a separate callback-faithful tilemap player selected before
    # the generated interpreter. Keeping it explicit still prevents fallback.
    "AnimTask_CreateSurfWave": ("SurfWave", "Screen", 136, True),
    "AnimTask_StartSlidingBg":
        ("SlidingBackground", "Screen", 0xFFFF, False),
    "AnimTask_SetPsychicBackground":
        ("PsychicPaletteCycle", "Screen", 0xFFFF, False),
    "AnimTask_FadeScreenToWhite":
        ("FadeScreenPaletteCycle", "Screen", 0xFFFF, False),
    "AnimTask_LoadSandstormBackground":
        ("SandstormField", "Screen", 159, True),
    "AnimTask_HazeScrollingFog":
        ("HazeFogField", "Screen", 195, True),
    "AnimTask_MistBallFog":
        ("MistBallFogField", "Screen", 122, True),
    "AnimTask_HeartsBackground":
        ("HeartsField", "Screen", 271, True),
    "AnimTask_MorningSunLightBeam":
        ("MorningSunField", "Attacker", 164, True),
    "AnimTask_CreateSpotlight":
        ("SpotlightWindow", "Screen", 1, True),
    "AnimTask_RemoveSpotlight":
        ("SpotlightWindow", "Screen", 1, True),
    "AnimTask_ExtrasensoryDistortion":
        ("ExtrasensoryDistortion", "Target", 26, True),
    "AnimTask_DragonDanceWaver":
        ("DragonDanceDistortion", "Attacker", 76, True),
    "AnimTask_MoveHeatWaveTargets":
        ("HeatWaveTargetMotion", "Target", 129, True),
    "AnimTask_SketchDrawMon":
        ("SketchScanline", "Attacker", 96, True),
    "AnimTask_MoveAttackerMementoShadow":
        ("MementoAttackerShadow", "Attacker", 96, True),
    "AnimTask_MoveTargetMementoShadow":
        ("MementoTargetShadow", "Target", 112, True),
    "AnimTask_MementoHandleBg":
        ("MementoBackgroundControl", "Screen", 1, True),
    "AnimTask_PositionFissureBgOnBattler":
        ("FissureBackgroundPosition", "Screen", 0xFFFF, False),
    "AnimTask_MoveSeismicTossBg":
        ("SeismicTossBackground", "Screen", 121, True),
    "AnimTask_SeismicTossBgAccelerateDownAtEnd":
        ("SeismicTossBackgroundEnd", "Screen", 0xFFFF, True),
    "AnimTask_MoveSkyUppercutBg":
        ("SkyUppercutBackground", "Screen", 0xFFFF, True),
    "AnimTask_SetAttackerInvisibleWaitForSignal":
        ("PersistentAttackerHide", "Attacker", 0xFFFF, False),
}

TASK_MOTION_LIFETIMES = {
    "None": 1, "ShakeHorizontal": 16, "ShakeVertical": 16,
    "ShakeSink": 24, "ShakePattern": 18, "Horizontal": 18,
    "Vertical": 18, "Lunge": 16, "Elliptical": 24, "Bounce": 24,
    "Sway": 24, "Spin": 20, "SlideOffscreen": 24, "Sink": 24,
    "Hide": 18, "Reveal": 18, "Teleport": 24, "ScalePulse": 24,
    "StretchVertical": 20, "Squish": 20, "Shrink": 24, "Grow": 24,
    "ClonePulse": 24, "Transform": 24, "PaletteFade": 20,
    "PaletteFlash": 16, "PaletteInvert": 16, "PaletteGrayscale": 20,
    "PaletteCycle": 24, "PaletteTint": 20,
    "TagPaletteRotate": 96, "TagBlendCycle": 192,
    "TagBlendInOut": 192, "TagBlendToColor": 96,
    "TagFlash": 192, "NightSlashPalette": 1,
    "MagicalLeafPaletteCycle": 0xFFFF,
    "MusicNotePaletteSetup": 1, "MusicNotePaletteClear": 1,
    "PaletteBackupSave": 1, "PaletteBackupRestore": 1,
    "PaletteBackupCommit": 1, "CurseWhiteLines": 64,
    "DrillPeckHitSplats": 33, "SolarBeamOrbs": 185,
    "ElectricBoltSegments": 25, "SmokescreenImpact": 42,
    "GlareEyeDots": 89, "BarrageBall": 58, "BestowItemThrow": 41,
    "DestinyBondShadow": 120, "RolloutDebris": 100,
    "Raindrops": 134, "SpeedDust": 132, "HailStones": 150,
    "TormentThoughtBubbles": 132, "ElectricChargingParticles": 150,
    "EruptionLaunchRocks": 112, "SkillSwapOrbs": 104,
    "ImprisonOrbs": 82, "GrudgeFlames": 110,
    "AirCutterProjectiles": 72, "WaterSpoutLaunch": 110,
    "WaterSpoutRain": 110, "FrozenIceCube": 132,
    "VoltTackleBolts": 72, "WaterSportOrbs": 170,
    "LeafBladePath": 150, "ShockWaveProgressingBolt": 150,
    "ShockWaveLightning": 80,
    "IonRain": 120,
}

# Conservative source-timeline defaults for callback-identity profiles.
# spawnBattleAnimationTask derives argument-dependent lifetimes at runtime;
# raw task-affine streams extend these values during generation.  Keeping an
# explicit row for every callback makes a new visual task fail generation
# instead of silently inheriting an unrelated generic animation.
TASK_MOTION_LIFETIMES.update({
    "AcidArmor": 112, "AttackerStretchAndDisappear": 24,
    "CompressTargetHorizontally": 96, "CompressTargetHorizontallyFast": 64,
    "CurseStretchingBlackBg": 32, "DeepInhale": 32,
    "DefenseCurlDeformMon": 32, "DigDownMovement": 64,
    "DigUpMovement": 16, "DoubleTeam": 96,
    "ExtremeSpeedImpact": 24, "ExtremeSpeedMonReappear": 24,
    "FlailMovement": 36, "GrowAndGrayscale": 81,
    "GrowAndShrink": 32, "HelpingHandAttackerMovement": 48,
    "MeditateStretchAttacker": 32, "Minimize": 160,
    "MonToSubstitute": 48, "NightShadeClone": 80,
    "NightmareClone": 64, "OdorSleuthMovement": 96,
    "PainSplitMovement": 14, "RapidSpinMonElevation": 32,
    "RockMonBackAndForth": 64, "RolePlaySilhouette": 80,
    "RotateAuroraRingColors": 64, "RotateMonSpriteToSide": 32,
    "RotateVerticallyExact": 161,
    "SnatchOpposingMonMoveExact": 80,
    "RotateMonToSideAndRestore": 64, "ScaleMonAndRestore": 32,
    "SetAllNonAttackersInvisible": 1,
    "SetAttackerInvisibleWaitForSignal": 0xFFFF,
    "SetGrayscaleOrOriginalPal": 1, "ShrinkTargetCopy": 64,
    "SkullBashPosition": 48, "SlackOffSquish": 32,
    "SlideOffScreenExact": 64, "SmellingSaltsSquish": 64,
    "SpitUpDeformMon": 32, "Splash": 64,
    "SquishAndSweatDroplets": 64, "StockpileDeformMon": 32,
    "StretchAttackerUp": 32, "StretchTargetUp": 32,
    "StrongFrustrationGrowAndShrink": 64, "SwallowDeformMon": 32,
    "SwayMonExact": 64, "TeeterDanceMovement": 128,
    "TeleportExact": 32, "ThrashMoveMonHorizontal": 64,
    "ThrashMoveMonVertical": 192, "TransformMon": 68,
    "TranslateMonElliptical": 64,
    "TranslateMonEllipticalRespectSide": 64,
    "TransparentCloneGrowAndShrink": 64, "UproarDistortion": 64,
    "VoltTackleAttackerReappear": 16, "WindUpLunge": 64,
    "WithdrawExact": 32,
    "HorizontalShakeExact": 32, "ShakeAndSinkMonExact": 32,
    "ShakeBattleTerrainExact": 32, "ShakeMonExact": 32,
    "ShakeMon2Exact": 32, "ShakeMonInPlaceExact": 32,
    "ShakeTargetPowerOrDamage": 32, "ShakeTargetInPatternExact": 64,
    "AlphaFadeInExact": 32, "AttackerFadeFromInvisibleExact": 32,
    "AttackerFadeToInvisibleExact": 32, "AttackerPunchWithTraceExact": 24,
    "BlendBackgroundExact": 32, "BlendBattleAnimPalExact": 64,
    "BlendBattleAnimPalExcludeExact": 64, "BlendColorCycleExact": 128,
    "BlendColorCycleExcludeExact": 128, "BlendMonInAndOutExact": 96,
    "BlendNonAttackerPalettesExact": 32, "Conversion2AlphaBlendExact": 64,
    "ConversionAlphaBlendExact": 64, "FacadeColorBlendExact": 48,
    "FadeScreenToWhiteExact": 32, "FakeOutExact": 20,
    "FlashExact": 64, "HardwarePaletteFadeExact": 64,
    "InitAttackerFadeFromInvisibleExact": 32, "InitMementoShadowExact": 32,
    "InvertScreenColorExact": 1, "MetallicShineExact": 64,
    "MoonlightEndFadeExact": 64, "ScaryFaceExact": 64,
    "SetCamouflageBlendExact": 32, "SpiteTargetShadowExact": 136,
    "StatusClearedEffectExact": 32, "TraceMonBlendedExact": 64,
})


def task_motion_resource(name: str, kind: str,
                         has_exact_sprites: bool) -> tuple[str, str, int, bool]:
    kind_name = kind.split("::")[-1]
    background = TASK_BACKGROUND_RESOURCES.get(name)
    if background is not None:
        return background
    mapping = TASK_EXACT_SPRITE_RESOURCES.get(name)
    if mapping is None and kind_name in {"BattlerMotion", "Effect"}:
        # A few expansion scripts classify a source affine deformation as an
        # Effect even though the retained renderer owns it as a battler
        # motion (UproarDistortion is the first example).  Only an explicit
        # audited table entry may cross this boundary.
        mapping = TASK_MOTION_RESOURCES.get(name)
    elif mapping is None and kind_name == "Palette":
        mapping = TASK_PALETTE_RESOURCES.get(name)
    elif mapping is None and kind_name == "Shake":
        mapping = TASK_SHAKE_RESOURCES.get(name)
    if mapping is None:
        if kind_name in {"Control", "Sound"}:
            mapping = ("None", "None")
        elif kind_name.endswith("Background") or kind_name == "Background":
            raise RuntimeError(
                f"Background task lacks source-faithful semantics: {name}")
        elif kind_name == "Effect" and has_exact_sprites:
            raise RuntimeError(f"Exact task pattern was not audited: {name}")
    if mapping is None:
        raise RuntimeError(
            f"Visual task lacks explicit retained semantics: {name} ({kind_name})")
    motion, target = mapping
    # Magical Leaf owns a palette cycle until its script writes -1 to arg 7;
    # like FireRed, it must not block waitforvisualfinish on its own. Palette
    # setup/backup tasks are explicit one-tick state operations rather than
    # visual fallbacks.
    blocking = motion != "MagicalLeafPaletteCycle"
    return motion, target, TASK_MOTION_LIFETIMES[motion], blocking


rows: list[str] = []
program_rows: list[str] = []
program_command_rows: list[str] = []
program_argument_values: list[int] = []
program_argument_offsets: dict[tuple[int, ...], int] = {}
sprite_resource_rows: list[str] = []
sprite_frame_step_rows: list[str] = []
sprite_frame_sequence_rows: list[str] = []
sprite_affine_command_rows: list[str] = []
sprite_affine_sequence_rows: list[str] = []
task_resource_rows: list[str] = []
# Asset compatibility deliberately excludes firmware-only renderer metadata.
# Keep the legacy row shape here so adding direction/interpolation fields does
# not make an unchanged SD pack look incompatible.
sprite_asset_catalog_rows: list[str] = []
sprite_resource_ids: dict[str, int] = {}
task_resource_ids: dict[str, int] = {}
audit: dict[str, object] = {
    "moves": {}, "visualAssets": {}, "unsupportedCommands": {},
    "taskHandlers": {}, "spriteHandlers": {},
    "program": {"commands": 0, "arguments": 0, "spriteResources": 0},
}
visual_assets: dict[str, dict[str, object]] = audit["visualAssets"]  # type: ignore[assignment]
unsupported_commands: dict[str, int] = audit["unsupportedCommands"]  # type: ignore[assignment]
task_handler_audit: dict[str, dict[str, object]] = audit["taskHandlers"]  # type: ignore[assignment]
sprite_handler_audit: dict[str, dict[str, object]] = audit["spriteHandlers"]  # type: ignore[assignment]


def move_script_label(symbol: str, number: int) -> str:
    if number < 355:
        return f"Move_{symbol}"
    normalized = re.sub(r"[^a-z0-9]", "", symbol.lower())
    candidates = [label for label in LABEL_BODIES
                  if label.startswith("gBattleAnimMove_") and
                  re.sub(r"[^a-z0-9]", "", label[len("gBattleAnimMove_"):].lower())
                  == normalized]
    if len(candidates) != 1:
        raise RuntimeError(
            f"Expansion move {symbol} ({number}) does not resolve to one source label: {candidates}")
    return candidates[0]


def sprite_animation_selector(callback: str,
                              sequence_count: int) -> tuple[int, int, str]:
    """Return (argument index, constant, proof) for StartSpriteAnim.

    An argument index of -1 means the constant is used. FireRed callbacks
    usually select direction/orientation directly from gBattleAnimArgs; the
    old renderer flattened those alternatives and cycled through all of them.
    """
    if sequence_count <= 1:
        return -1, 0, "SingleSequence"
    manual_selector = MANUAL_TILE_ARGUMENT_VARIANTS.get(callback)
    if manual_selector is not None:
        argument, _, count, _ = manual_selector
        if sequence_count != count:
            raise RuntimeError(
                f"Manual OAM selector sequence mismatch for {callback}: "
                f"{sequence_count} != {count}")
        return argument, 0, f"OamTileArgument{argument}"
    body = expanded_sprite_initializer_body(callback)
    argument_matches = {
        int(value) for value in re.findall(
            r"StartSpriteAnim\s*\(\s*sprite\s*,\s*"
            r"gBattleAnimArgs\s*\[\s*(\d+)\s*\]", body)
    }
    if len(argument_matches) == 1:
        argument = next(iter(argument_matches))
        return argument, 0, f"Argument{argument}"
    constant_matches = {
        int(value, 0) for value in re.findall(
            r"StartSpriteAnim\s*\(\s*sprite\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)",
            body)
    }
    if len(constant_matches) == 1:
        constant = next(iter(constant_matches))
        return -1, constant, f"Constant{constant}"
    # Sequence zero is SpriteTemplate's actual initial animNum. Dynamic
    # callbacks that change animNum later retain an explicit audit label; they
    # are never again mistaken for a frame carousel.
    return -1, 0, "DefaultInitialSequence"


def sprite_affine_selector(callback: str,
                           sequence_count: int) -> tuple[int, int, str]:
    if sequence_count <= 1:
        return -1, 0, "SingleSequence" if sequence_count else "NoAffine"
    body = expanded_sprite_initializer_body(callback)
    argument_matches = {
        int(value) for value in re.findall(
            r"StartSpriteAffineAnim\s*\(\s*sprite\s*,\s*"
            r"gBattleAnimArgs\s*\[\s*(\d+)\s*\]", body)
    }
    if len(argument_matches) == 1:
        argument = next(iter(argument_matches))
        return argument, 0, f"Argument{argument}"
    constant_matches = {
        int(value, 0) for value in re.findall(
            r"StartSpriteAffineAnim\s*\(\s*sprite\s*,\s*"
            r"(0x[0-9A-Fa-f]+|\d+)\s*\)", body)
    }
    if len(constant_matches) == 1:
        constant = next(iter(constant_matches))
        return -1, constant, f"Constant{constant}"
    return -1, 0, "DefaultInitialSequence"


def finite_frame_sequence_duration(sequence: dict[str, object]) -> int:
    """Return the source duration, or zero for an intentionally looping cel."""
    steps = list(sequence.get("steps", []))
    if int(sequence.get("loopStep", 0xFF)) != 0xFF:
        return 0
    return sum(max(1, int(step["duration"])) for step in steps)


def finite_affine_sequence_duration(sequence: dict[str, object]) -> int:
    """Return the source affine duration, or zero for an infinite loop.

    The GBA consumes a duration-zero AFFINEANIMCMD_FRAME as an immediate
    absolute setup command.  It still needs one retained renderer tick so the
    configured transform can be presented before the following command.
    """
    commands = list(sequence.get("commands", []))
    if int(sequence.get("loopCommand", 0xFF)) != 0xFF:
        return 0
    return sum(max(1, int(command["duration"]) & 0xFF)
               for command in commands)


def append_affine_sequence(sequence: dict[str, object], owner: str) -> int:
    """Append one parsed FireRed affine stream and return its table index."""
    commands = list(sequence.get("commands", []))
    if not commands:
        raise RuntimeError(f"Empty affine command stream: {owner}")
    command_offset = len(sprite_affine_command_rows)
    for command in commands:
        x_scale = ((int(command["x"]) + 0x8000) & 0xFFFF) - 0x8000
        y_scale = ((int(command["y"]) + 0x8000) & 0xFFFF) - 0x8000
        rotation = ((int(command["rotation"]) + 0x80) & 0xFF) - 0x80
        duration = int(command["duration"]) & 0xFF
        sprite_affine_command_rows.append(
            f"  {{{x_scale}, {y_scale}, {rotation}, {duration}}},")
    loop_command = int(sequence.get("loopCommand", 0xFF))
    if loop_command != 0xFF and not 0 <= loop_command < len(commands):
        raise RuntimeError(
            f"Invalid affine loop for {owner}: {loop_command}/{len(commands)}")
    sequence_index = len(sprite_affine_sequence_rows)
    sprite_affine_sequence_rows.append(
        f"  {{{command_offset}, {len(commands)}, {loop_command}}},")
    return sequence_index


def source_driven_sprite_lifetime(
        callback: str, baseline: int,
        sequences: list[dict[str, object]],
        affine_sequences: list[dict[str, object]]) -> tuple[int, str]:
    """Keep a retained sprite alive for the completion condition in FireRed.

    Motion profiles describe *where* a sprite travels; they are not a valid
    substitute for the ANIMCMD/affine timeline that decides *when* the source
    callback destroys it.  This was the systemic reason that long effects
    disappeared half-way through even though every cel existed on the SD.

    Initial callbacks and every installed state are inspected.  We only
    extend a resource when that state machine explicitly waits for the normal
    or affine animation to finish, so timer-destroyed particles do not linger
    merely because their SpriteTemplate happens to reference a longer atlas.
    """
    lifecycle = expanded_sprite_motion_body(callback)
    waits_for_frames = bool(re.search(
        r"RunStoredCallbackWhenAnimEnds|(?<![A-Za-z0-9_])animEnded\b",
        lifecycle))
    waits_for_affine = bool(re.search(
        r"RunStoredCallbackWhenAffineAnimEnds|\baffineAnimEnded\b",
        lifecycle))
    frame_duration = max(
        (finite_frame_sequence_duration(sequence) for sequence in sequences),
        default=0)
    affine_duration = max(
        (finite_affine_sequence_duration(sequence)
         for sequence in affine_sequences), default=0)

    derived = baseline
    proofs: list[str] = []
    if waits_for_frames and frame_duration:
        derived = max(derived, frame_duration)
        proofs.append(f"frame={frame_duration}")
    if waits_for_affine and affine_duration:
        derived = max(derived, affine_duration)
        proofs.append(f"affine={affine_duration}")

    # Some state machines wait for the normal cel stream, install an affine
    # animation in the next callback and then wait for that second stream.
    # Their phases are sequential.  A single `animEnded || affineAnimEnded`
    # condition instead represents concurrent alternatives and must use max.
    concurrent_wait = bool(re.search(
        r"animEnded\s*\|\|\s*sprite->affineAnimEnded|"
        r"affineAnimEnded\s*\|\|\s*sprite->animEnded", lifecycle))
    if (waits_for_frames and waits_for_affine and frame_duration and
            affine_duration and not concurrent_wait):
        derived = max(derived, frame_duration + affine_duration)
        proofs.append("sequential")

    # The compact firmware age is one byte. No finite FireRed source stream
    # exceeds it; fail instead of silently wrapping if a future source does.
    if derived > 255:
        raise RuntimeError(
            f"Sprite lifetime exceeds retained age: {callback} ({derived})")
    return derived, "+".join(proofs) if proofs else "MotionProfile"


def sprite_resource_id(template: str) -> int:
    existing = sprite_resource_ids.get(template)
    if existing is not None:
        return existing
    spec = template_specs.get(template)
    if spec is None:
        raise RuntimeError(f"Animation template is not defined in FireRed sources: {template}")
    tag = spec["tag"]
    template_source = str(spec.get("source", "firered"))
    palette_tag = 0
    if tag:
        tag_constant = f"ANIM_TAG_{tag}"
        # A handful of task-private templates use TAG_* constants local to a
        # C file (Smokescreen, enemy shadow, etc.). No script palette task can
        # address those through ANIM_TAG_*, so zero is the correct compact
        # representation. Public animation tags must retain their exact ID.
        if tag_constant not in EXPRESSION_CONSTANTS and tag not in PRIVATE_SYNTHETIC_TAGS:
            raise RuntimeError(
                f"Public battle animation palette tag is unresolved: "
                f"{template} / {tag_constant}")
        palette_tag = EXPRESSION_CONSTANTS.get(tag_constant, 0)
    callback = spec["callback"]
    if tag:
        asset = template_visual_asset_spec(template, tag, template_source)
        if asset is None and tag == "SMOKESCREEN_IMPACT":
            # Smokescreen's private TAG_* sheet has no normal SpriteTemplate
            # ANIM_TAG entry. Its source and 16x16 OAM contract are explicit
            # above, so reuse that exact private asset rather than dropping it
            # into the generic HIT family.
            asset = visual_asset_spec(tag, source_name=template_source,
                                      palette_source_name=template_source)
            asset["template"] = template
        if asset is None:
            asset = visual_asset_spec(tag, source_name=template_source,
                                      palette_source_name=template_source)
        if asset.get("template") != template:
            raise RuntimeError(
                f"Tagged SpriteTemplate lacks an exact cel export: {template} / {tag}")
        visual_assets[asset["key"]] = asset  # type: ignore[index]
        asset_key = str(asset["key"])
        frames = int(asset["frameCount"])
        sequences = list(asset.get("sequences", []))
    else:
        asset_key = ""
        frames = 1
        sequences = [{"steps": [{"frame": 0, "duration": 1}],
                      "loopStep": 0xFF}]
    if not sequences:
        sequences = [{"steps": [{"frame": 0, "duration": 1}],
                      "loopStep": 0xFF}]
    sequence_offset = len(sprite_frame_sequence_rows)
    for sequence in sequences:
        steps = list(sequence["steps"])
        if not steps:
            steps = [{"frame": 0, "duration": 1}]
        step_offset = len(sprite_frame_step_rows)
        for step in steps:
            sprite_frame_step_rows.append(
                f"  {{{int(step['frame'])}, {max(1, int(step['duration']))}}},")
        loop_step = int(sequence.get("loopStep", 0xFF))
        if loop_step != 0xFF and not 0 <= loop_step < len(steps):
            raise RuntimeError(
                f"Invalid frame loop for {template}: {loop_step}/{len(steps)}")
        sprite_frame_sequence_rows.append(
            f"  {{{step_offset}, {len(steps)}, {loop_step}}},")
    selector_argument, selector_constant, selector_proof = \
        sprite_animation_selector(callback, len(sequences))
    affine_table = str(spec.get("affineAnimations", ""))
    lifecycle_body = expanded_sprite_motion_body(callback)
    manual_tile_write = "oam.tileNum" in lifecycle_body
    manual_tile_proof = (callback in MANUAL_TILE_BASE_BIAS or
                         callback in MANUAL_TILE_ARGUMENT_VARIANTS or
                         callback in MANUAL_TILE_PHASE_VARIANTS)
    if manual_tile_write and not manual_tile_proof:
        raise RuntimeError(
            f"Sprite callback mutates OAM tileNum without an extracted cel "
            f"plan: {template}/{callback}")
    installed_affine_tables = sorted(set(re.findall(
        r"sprite->affineAnims\s*=\s*(\w+)", lifecycle_body)))
    affine_tables: list[str] = []
    if affine_table and "Dummy" not in affine_table:
        affine_tables.append(affine_table)
    for installed_table in installed_affine_tables:
        if installed_table not in affine_tables:
            affine_tables.append(installed_table)
    for table in affine_tables:
        if not affine_pointer_tables.get(table, []):
            raise RuntimeError(
                f"Sprite callback affine table was not parsed: {template}/{table}")
    affine_arrays = [array for table in affine_tables
                     for array in affine_pointer_tables.get(table, [])]
    affine_sequences = [affine_command_arrays[name] for name in affine_arrays
                        if name in affine_command_arrays]
    affine_sequence_offset = len(sprite_affine_sequence_rows)
    for sequence in affine_sequences:
        append_affine_sequence(sequence, template)
    affine_sequence_count = len(sprite_affine_sequence_rows) - affine_sequence_offset
    affine_selector_argument, affine_selector_constant, affine_selector_proof = \
        sprite_affine_selector(callback, affine_sequence_count)
    # A table assigned by a later state does not exist at sprite creation.
    # Its first transform begins after the finite normal animation that gates
    # that state. Keep this explicit in the compact resource instead of
    # applying the late transform to the spawn frame.
    affine_start_age = 0
    if installed_affine_tables and not (affine_table and "Dummy" not in affine_table):
        affine_start_age = max(
            (finite_frame_sequence_duration(sequence) for sequence in sequences),
            default=0)
    motion, profile_lifetime = sprite_motion(template, callback, tag)
    lifetime, lifetime_proof = source_driven_sprite_lifetime(
        callback, profile_lifetime, sequences, affine_sequences)
    travel = sprite_travel(template, callback, motion)
    origin = sprite_origin(template, callback, travel)
    # Most shared positioning helpers use arg0/arg1, but several callbacks
    # put a battler selector or another control value before the real offsets.
    # The renderer must never guess this ABI from the values at runtime.
    offset_argument_overrides = {
        "AnimDragonRageFirePlume": (1, 2),
        "AnimThunderboltOrb": (1, 2),
        "AnimAngerMark": (1, 2),
        "AnimHitSplatOnMonEdge": (1, 2),
        "AnimSlashSlice": (1, 2),
        "SpriteCB_SpriteOnMonForDuration": (1, 2),
        "AnimElectricPuff": (1, 2),
        "AnimFrenzyPlantRoot": (1, 2),
        "AnimAssistPawprint": (-1, -1),
        "AnimSmellingSaltExclamation": (-1, -1),
        "AnimMetronomeFinger": (-1, -1),
        "AnimMilkBottle": (-1, -1),
        "AnimRecycle": (-1, -1),
        "AnimRedX": (-1, -1),
        "AnimSharpenSphere": (-1, -1),
        "AnimSpiderWeb": (-1, -1),
        "AnimWhiteHalo": (-1, -1),
        "AnimDigDirtMound": (-1, -1),
        "AnimMovementWaves": (-1, -1),
        "AnimSparkElectricity": (-1, -1),
        "AnimTailGlowOrb": (-1, -1),
        "AnimGrowingChargeOrb": (-1, -1),
        "AnimGrowingShockWaveOrb": (-1, -1),
        "AnimDiveWaterSplash": (-1, -1),
        "AnimBellyDrumHand": (-1, -1),
        "AnimQuestionMark": (-1, -1),
        "AnimateZenHeadbutt": (-1, -1),
    }
    x_offset_argument, y_offset_argument = offset_argument_overrides.get(
        callback, (0, 1))
    # A generic local projectile has identical source and destination in the
    # firmware renderer, so it is not an approximation: it is a stationary
    # sprite.  Refuse to generate that false-positive coverage class. Every
    # local translation must declare its callback-specific motion above.
    if (motion == "BattleAnimationMotion::Projectile" and
            travel == "BattleAnimationTravel::Local"):
        raise RuntimeError(
            f"Stationary generic projectile requires an explicit motion: "
            f"{template} / {callback}")
    state_callbacks = installed_sprite_callbacks(callback)
    state_moves_coordinates = installed_sprite_moves_coordinates(callback)
    coverage = sprite_motion_coverage(callback, motion, state_moves_coordinates)
    sprite_handler_audit[template] = {
        "callback": callback,
        "tag": tag,
        "asset": asset_key,
        "frames": frames,
        "sequences": len(sequences),
        "frameSelector": selector_proof,
        "affineSequences": affine_sequence_count,
        "affineSelector": affine_selector_proof,
        "affineStartAge": affine_start_age,
        "motion": motion.split("::")[-1],
        "travel": travel.split("::")[-1],
        "origin": origin.split("::")[-1],
        "xOffsetArgument": x_offset_argument,
        "yOffsetArgument": y_offset_argument,
        "stateCallbacks": state_callbacks,
        "stateMovesCoordinates": state_moves_coordinates,
        "manualTileWrite": manual_tile_write,
        "manualTileExtracted": manual_tile_proof,
        "coverage": coverage,
        "profileLifetime": profile_lifetime,
        "lifetime": lifetime,
        "lifetimeProof": lifetime_proof,
    }
    identifier = len(sprite_resource_rows)
    # AnimFistOrFootRandomPos creates gBasicHitSplatSpriteTemplate directly
    # from the primary sprite.  The GBA therefore displays both cels at the
    # same sampled impact position; dropping the child is what made
    # Close Combat look like a bare placeholder on the retained renderer.
    child_resource_id = 0xFFFF
    if template == "gFistFootRandomPosSpriteTemplate":
        child_resource_id = sprite_resource_id("gBasicHitSplatSpriteTemplate")
    sprite_resource_ids[template] = identifier
    sprite_resource_rows.append(
        f'  {{"{asset_key}", {palette_tag}, {frames}, {sequence_offset}, {len(sequences)}, '
        f'{selector_argument}, {selector_constant}, {motion}, {travel}, {origin}, '
        f'{x_offset_argument}, {y_offset_argument}, {lifetime}, '
        f'{affine_sequence_offset}, {affine_sequence_count}, '
        f'{affine_selector_argument}, {affine_selector_constant}, {affine_start_age}, '
        f'{child_resource_id}}}, '
        f'// {template} / {callback}')
    sprite_asset_catalog_rows.append(
        f'  {{"{asset_key}", {frames}, {motion}, {lifetime}}}, // {template} / {callback}')
    return identifier


def task_resource_id(name: str, kind: str,
                     task_templates: list[str]) -> int:
    existing = task_resource_ids.get(name)
    if existing is not None:
        return existing
    motion, target, lifetime, blocking = task_motion_resource(
        name, kind, bool(task_templates))
    if kind.endswith("::Palette") and motion == "None":
        raise RuntimeError(f"Palette task has no explicit semantics: {name}")
    sprite_ids = [sprite_resource_id(template) for template in task_templates[:2]]
    sprite_ids += [0xFFFF] * (2 - len(sprite_ids))
    # PrepareAffineAnimInTaskData consumes a raw AffineAnimCmd array rather
    # than a SpriteTemplate table. Parse and retain that exact stream too.
    task_body = expanded_task_callback_body(name)
    task_affine_names = list(dict.fromkeys(re.findall(
        r"PrepareAffineAnimInTaskData\s*\([^;]*?,\s*"
        r"([A-Za-z_][A-Za-z0-9_]*)\s*\)", task_body, re.S)))
    task_affine_names = [value for value in task_affine_names
                         if value in affine_command_arrays]
    if len(task_affine_names) > 1:
        raise RuntimeError(
            f"Task switches between unaudited affine streams: "
            f"{name} / {task_affine_names}")
    task_affine_offset = len(sprite_affine_sequence_rows)
    task_affine_count = 0
    if task_affine_names:
        sequence = affine_command_arrays[task_affine_names[0]]
        append_affine_sequence(sequence, name)
        task_affine_count = 1
        finite_duration = finite_affine_sequence_duration(sequence)
        if finite_duration:
            lifetime = max(lifetime, finite_duration)
    identifier = len(task_resource_rows)
    task_resource_ids[name] = identifier
    task_resource_rows.append(
        f"  {{{kind}, BattleAnimationTaskMotion::{motion}, "
        f"BattleAnimationTaskTarget::{target}, {lifetime}, "
        f"{{{sprite_ids[0]}, {sprite_ids[1]}}}, "
        f"{min(2, len(task_templates))}, "
        f"{task_affine_offset}, {task_affine_count}, 0, "
        f"{'true' if blocking else 'false'}}}, // {name}")
    return identifier


def append_program_command(opcode: str, resource: str = "0", anchor: int = 0,
                           priority: int = 0, arguments: list[int] | None = None) -> None:
    values = arguments or []
    key = tuple(values)
    offset = program_argument_offsets.get(key, -1)
    if offset < 0:
        offset = len(program_argument_values)
        program_argument_values.extend(values)
        program_argument_offsets[key] = offset
    program_command_rows.append(
        f"  {{{resource}, {offset}, static_cast<uint8_t>(BattleAnimationOpcode::{opcode}), "
        f"{anchor & 0xFF}, {priority & 0xFF}, {len(values)}}},")

move_definitions: list[tuple[str, int]] = [
    (symbol, int(number_text))
    for symbol, number_text in re.findall(
        r"#define MOVE_([A-Z0-9_]+)\s+(\d+)", MOVES)
    if 1 <= int(number_text) <= 354
]
expansion_move_definitions = [
    (symbol, int(number_text))
    for symbol, number_text in re.findall(
        r"\bMOVE_([A-Z0-9_]+)\s*=\s*(\d+)\s*,?",
        (EXPANSION / "include/constants/moves.h").read_text(encoding="utf-8"))
    if int(number_text) in EXPANSION_MOVE_IDS
]
# The first entry of each expansion generation aliases the preceding
# generation count instead of spelling out its numeric ID. Keep every boundary
# move in the exact catalog rather than silently losing it to the numeric-only
# parser.
if 355 in EXPANSION_MOVE_IDS:
    expansion_move_definitions.insert(0, ("ROOST", 355))
if 468 in EXPANSION_MOVE_IDS:
    expansion_move_definitions.append(("HONE_CLAWS", 468))
if 560 in EXPANSION_MOVE_IDS:
    expansion_move_definitions.append(("FLYING_PRESS", 560))
expansion_move_definitions.sort(key=lambda entry: entry[1])
move_definitions += expansion_move_definitions

for symbol, number in move_definitions:
    source_label = move_script_label(symbol, number)
    body, source_found = expanded_script(source_label)
    ordered_source = animation_source_lines(source_label)
    counting_body = "\n".join(ordered_source)

    tags: list[str] = []
    for tag in re.findall(r"loadspritegfx\s+ANIM_TAG_([A-Z0-9_]+)", body):
        if tag not in tags:
            tags.append(tag)
    tags = tags[:4]

    creates = len(re.findall(r"^\s*createsprite\b", counting_body, re.M))
    visual_tasks = len(re.findall(r"^\s*createvisualtask\b", counting_body, re.M))
    delays = [int(value) for value in re.findall(r"^\s*delay\s+(\d+)", counting_body, re.M)]

    flags: list[str] = []
    if "ShakeMon" in body:
        flags.append("AnimShakeTarget")
    if "ShakeBattleBG" in body or "ShakeBattleTerrain" in body:
        flags.append("AnimShakeScreen")
    if "setalpha" in body or "setbldcnt" in body:
        flags.append("AnimBlend")
    if "monbg" in body:
        flags.append("AnimTargetBackground")
    if re.search(r"AnimTask_(Translate|Squish|Wobble|Slide|Jump).*Attacker", body):
        flags.append("AnimAttackerMotion")
    if "BlendMonInAndOut" in body or "Flash" in body or "InvertScreenColor" in body:
        flags.append("AnimFlash")
    if re.search(r"\b(changebg|fadetobg|fadetobgfromset|splitbgprio|restorebg)\b", body):
        flags.append("AnimBackgroundChange")
    if re.search(r"^\s*invisible\b", body, re.M):
        flags.append("AnimHideBattler")

    specs = [visual_asset_spec(tag) for tag in tags]
    for spec in specs:
        visual_assets[spec["key"]] = spec  # type: ignore[index]
    # A task-only animation still needs enough runtime phases to be visible.
    # It must never be disguised as a HIT sprite merely to satisfy assetCount.
    particle = max(1, min(12, creates if creates else (visual_tasks or len(tags))))
    cadence = max(35, min(120, round(sum(delays) * 16.67 / max(1, particle)) if delays else 65))
    flag_expr = " | ".join(flags) if flags else "0"
    assets = [str(spec["key"]) for spec in specs] + [""] * (4 - len(specs))
    frames = [int(spec["frameCount"]) for spec in specs] + [0] * (4 - len(specs))
    rows.append(
        f'  {{{number}, {{"{assets[0]}", "{assets[1]}", "{assets[2]}", "{assets[3]}"}}, '
        f'{{{frames[0]}, {frames[1]}, {frames[2]}, {frames[3]}}}, '
        f'{len(specs)}, {particle}, {cadence}, {flag_expr}}},')

    command_start = len(program_command_rows)
    labels = reachable_animation_labels(source_label)
    move_sprite_templates = sorted({
        match.group(1)
        for label in labels
        for match in re.finditer(r"^\s*createsprite\s+(\w+)",
                                 LABEL_BODIES[label], re.M)
    })
    move_task_callbacks = sorted({
        match.group(1)
        for label in labels
        for match in re.finditer(r"^\s*createvisualtask\s+(\w+)",
                                 LABEL_BODIES[label], re.M)
    })
    move_commands: list[dict[str, object]] = []
    label_offsets: dict[str, int] = {}

    def emit(opcode: str, resource: str = "0", anchor: int = 0,
             priority: int = 0, arguments: list[int] | None = None,
             target: str | None = None) -> None:
        move_commands.append({
            "opcode": opcode, "resource": resource, "anchor": anchor,
            "priority": priority, "arguments": arguments or [],
            "target": target,
        })

    for label in labels:
        label_offsets[label] = len(move_commands)
        terminated = False
        for source_line in LABEL_BODIES[label].splitlines():
            line = source_line.split("@", 1)[0].strip()
            if not line:
                continue
            parts = line.split(None, 1)
            command = parts[0].lower()
            operands = split_operands(parts[1]) if len(parts) > 1 else []
            if command == "createsprite" and len(operands) >= 3:
                # As with visual tasks, expansion also permits the template
                # anchor to be separated from the first comma by whitespace.
                sprite_operands = operands
                first_parts = sprite_operands[0].split()
                if len(first_parts) >= 2:
                    sprite_operands = [first_parts[0], first_parts[1],
                                       *sprite_operands[1:]]
                emit("SpawnSprite", str(sprite_resource_id(sprite_operands[0])),
                     expression_value(sprite_operands[1]), expression_value(sprite_operands[2]),
                     [expression_value(value) for value in sprite_operands[3:]])
            elif command == "createvisualtask" and len(operands) >= 2:
                # Expansion accepts both `Task, priority, ...` and the
                # canonical assembler spelling `Task priority, ...`.  Keep
                # the priority in the normalized slot so both forms share
                # the same argument ABI.
                task_operands = operands
                first_parts = task_operands[0].split()
                if len(first_parts) >= 2:
                    task_operands = [first_parts[0], first_parts[1],
                                     *task_operands[1:]]
                task_name = task_operands[0]
                args = [expression_value(value) for value in task_operands[2:]]
                if task_name in QUERY_TASKS:
                    emit("Query", f"static_cast<uint16_t>({QUERY_TASKS[task_name]})",
                         arguments=args)
                    task_entry = task_handler_audit.setdefault(task_name, {
                        "kind": "Query", "templates": [],
                        "renderer": "FireRedRuntimeQuery", "uses": 0,
                    })
                    task_entry["uses"] = int(task_entry["uses"]) + 1
                    continue
                kind = task_kind(task_name)
                task_templates = task_sprite_templates(task_name)
                task_id = task_resource_id(task_name, kind, task_templates)
                task_motion, task_target, _, task_blocking = task_motion_resource(
                    task_name, kind, bool(task_templates))
                kind_name = kind.split("::")[-1]
                if task_templates:
                    renderer = "ExactFireRedSpriteTemplate"
                elif task_name == "AnimTask_CreateSurfWave":
                    renderer = "ExactSurfTilemap"
                elif kind_name == "Shake":
                    renderer = "RetainedBattlerShake"
                elif kind_name == "BattlerMotion":
                    renderer = "RetainedBattlerMotion"
                elif kind_name == "Palette":
                    renderer = "RetainedBattlerPalette"
                elif task_name in TASK_BACKGROUND_RESOURCES:
                    renderer = ("ExactTaskBackground" if task_motion in {
                        "SandstormField", "HazeFogField", "MistBallFogField",
                        "HeartsField", "MorningSunField"} else
                        "ExactBackgroundTransform" if task_motion in {
                        "SlidingBackground", "PsychicPaletteCycle",
                        "FadeScreenPaletteCycle", "FissureBackgroundPosition",
                        "SeismicTossBackground", "SeismicTossBackgroundEnd",
                        "SkyUppercutBackground"} else
                        "TaskWindowControl" if task_motion == "SpotlightWindow" else
                        "ExactBattlerScanline")
                elif kind_name in {"Control", "Sound"}:
                    renderer = "NonVisual"
                else:
                    renderer = "Unsupported"
                task_entry = task_handler_audit.setdefault(task_name, {
                    "kind": kind_name, "templates": task_templates,
                    "renderer": renderer, "motion": task_motion,
                    "target": task_target, "blocking": task_blocking,
                    "uses": 0,
                })
                task_entry["uses"] = int(task_entry["uses"]) + 1
                anchor = args[0] if args and 0 <= args[0] <= 3 else 1
                emit("VisualTask", str(task_id), anchor,
                     expression_value(operands[1]) | (0x80 if task_templates else 0), args)
                # Tasks that allocate OBJ internally retain those exact
                # SpriteTemplate ids in BattleAnimationTaskResource. Runtime
                # reproduces their burst/orbit/rain/projectile pattern rather
                # than emitting one frozen duplicate here.
            elif command == "delay" and operands:
                emit("Delay", arguments=[expression_value(operands[0])])
            elif command == "waitforvisualfinish":
                emit("WaitForVisuals")
            elif command == "waitbgfadeout":
                # FadeToBackground already performs and presents the fade-out
                # synchronously, stopping with the new BG hidden by black.
                continue
            elif command == "waitbgfadein":
                # Tasks between the two waits must start/stop while black,
                # exactly like FireRed. Reveal only at this source opcode.
                emit("FinishBackgroundFade")
            elif command in {"setalpha", "setbldcnt"}:
                emit("SetAlpha" if command == "setalpha" else "SetBlendControl",
                     arguments=[expression_value(value) for value in operands])
            elif command == "blendoff":
                emit("BlendOff")
            elif command in {"monbg", "monbg_static"}:
                emit("SetBattlerLayer", arguments=[
                    expression_value(operands[0]) if operands else 0])
            elif command in {"clearmonbg", "clearmonbg_static"}:
                emit("ClearBattlerLayer", arguments=[
                    expression_value(operands[0]) if operands else 0])
            elif command in {"splitbgprio", "splitbgprio_foes", "splitbgprio_all"}:
                emit("SetBackgroundPriority", arguments=[
                    0 if command == "splitbgprio" else
                    1 if command == "splitbgprio_foes" else 2])
            elif command == "changebg":
                emit("ChangeBackground",
                     str(expression_value(operands[0]) if operands else 0))
            elif command == "fadetobg":
                emit("FadeToBackground",
                     str(expression_value(operands[0]) if operands else 0))
            elif command == "fadetobgfromset":
                # FireRed picks bg2 when the target is on the player side and
                # bg1 otherwise (bg3 is Contest-only).  Retain both singles
                # choices; runtime resolves the actual target side.
                emit("FadeToBackground",
                     str(expression_value(operands[0]) if operands else 0),
                     arguments=[expression_value(operands[1])]
                     if len(operands) > 1 else [])
            elif command == "restorebg":
                emit("FadeRestoreBackground")
            elif command in {"invisible", "visible"}:
                emit("SetVisibility", arguments=[
                    expression_value(operands[0]) if operands else 0,
                    0 if command == "invisible" else 1,
                ])
            elif command == "setarg" and len(operands) >= 2:
                emit("SetArgument", arguments=[expression_value(operands[0]),
                                                expression_value(operands[1])])
            elif command == "call" and operands:
                emit("Call", target=operands[0])
            elif command == "goto" and operands:
                emit("Jump", target=operands[0])
                terminated = True
                break
            elif command == "return":
                emit("Return")
                terminated = True
                break
            elif command == "jumpargeq" and len(operands) >= 3:
                emit("JumpIfArgumentEquals",
                     arguments=[expression_value(operands[0]), expression_value(operands[1])],
                     target=operands[2])
            elif command == "jumprettrue" and operands:
                emit("JumpIfArgumentEquals", arguments=[7, 1], target=operands[0])
            elif command == "jumpretfalse" and operands:
                emit("JumpIfArgumentEquals", arguments=[7, 0], target=operands[0])
            elif command == "jumpreteq" and len(operands) >= 2:
                emit("JumpIfArgumentEquals", arguments=[7, expression_value(operands[0])],
                     target=operands[1])
            elif command == "jumpifmoveturn" and len(operands) >= 2:
                emit("JumpIfMoveTurnEquals", arguments=[expression_value(operands[0]), -1],
                     target=operands[1])
            elif command == "choosetwoturnanim":
                # The GBA selects the second address on odd animation turns.
                # Multi-hit moves can exceed turn 1, hence the explicit mask.
                choices = [token for token in re.findall(
                    r"[A-Za-z_][A-Za-z0-9_]*", parts[1] if len(parts) > 1 else "")
                    if token in LABEL_BODIES]
                if len(choices) < 2:
                    raise RuntimeError(
                        f"Move {symbol} has malformed choosetwoturnanim: {line}")
                emit("JumpIfMoveTurnEquals", arguments=[1, 1], target=choices[1])
                emit("Jump", target=choices[0])
                terminated = True
                break
            elif command == "jumpifcontest":
                # Pokegochi has no Contest presentation. Preserve normal
                # battle fall-through while still compiling the target label
                # for complete source/asset auditing.
                continue
            elif command == "end":
                emit("End")
                terminated = True
                break
        if not terminated and has_physical_fallthrough(label):
            emit("Jump", target=LABEL_NEXT[label])

    if not move_commands:
        emit("End")
    for generated in move_commands:
        target = generated["target"]
        resource = str(generated["resource"])
        if target is not None:
            if target not in label_offsets:
                raise RuntimeError(f"Move {symbol} references unreachable label {target}")
            resource = str(label_offsets[str(target)])
        append_program_command(
            str(generated["opcode"]), resource,
            int(generated["anchor"]), int(generated["priority"]),
            list(generated["arguments"]))  # type: ignore[arg-type]
    if not any(generated["opcode"] == "End" for generated in move_commands):
        append_program_command("End")
    move_command_count = len(program_command_rows) - command_start
    move_spawn_count = sum(
        "BattleAnimationOpcode::SpawnSprite" in command_row
        for command_row in program_command_rows[command_start:])
    script_static_sprites = sum(
        len(re.findall(r"^\s*createsprite\b", LABEL_BODIES[label], re.M))
        for label in labels)
    if move_spawn_count < script_static_sprites:
        raise RuntimeError(
            f"Move {symbol} lost sprite commands: script={script_static_sprites}, "
            f"program={move_spawn_count}")
    program_rows.append(
        f"  {{{number}, {command_start}, {move_command_count}}},")

    commands = re.findall(r"^\s*([a-z][a-z0-9_]*)\b", body, re.M)
    supported = {
        "loadspritegfx", "unloadspritegfx", "createsprite", "createvisualtask", "delay",
        "waitforvisualfinish", "monbg", "clearmonbg", "clearmonbg_static", "monbg_static",
        "setalpha", "blendoff", "setbldcnt", "playsewithpan", "loopsewithpan", "panse_1B",
        "panse_19", "waitplaysewithpan", "playse", "createsoundtask", "panse",
        "panse_adjustnone", "panse_adjustall", "waitsound", "setpan", "call", "return",
        "goto", "jumpargeq", "jumpifmoveturn", "jumpifcontest", "jumprettrue",
        "jumpretfalse", "jumpreteq", "choosetwoturnanim", "setarg", "changebg", "fadetobg",
        "fadetobgfromset", "waitbgfadeout", "waitbgfadein", "restorebg", "splitbgprio",
        "splitbgprio_foes", "splitbgprio_all", "invisible", "visible", "end",
    }
    unknown = sorted(set(commands) - supported)
    for command in unknown:
        unsupported_commands[command] = unsupported_commands.get(command, 0) + 1
    audit["moves"][str(number)] = {  # type: ignore[index]
        "symbol": symbol,
        "tags": tags,
        "assets": assets[:len(specs)],
        "particles": particle,
        "flags": flags,
        "sourceFound": source_found,
        "visualTasks": visual_tasks,
        "scriptSprites": script_static_sprites,
        "programSprites": move_spawn_count,
        "programCommands": move_command_count,
        "controlFlowLabels": len(labels),
        "conditionalBranches": sum(
            generated["opcode"] in {"JumpIfArgumentEquals", "JumpIfMoveTurnEquals"}
            for generated in move_commands),
        "implicitFallthrough": has_physical_fallthrough(source_label),
        "spriteTemplates": move_sprite_templates,
        "taskCallbacks": move_task_callbacks,
        "untranslated": unknown,
    }

    if symbol == "BLIZZARD":
        required_templates = {
            "gSwirlingSnowballSpriteTemplate",
            "gBlizzardIceCrystalSpriteTemplate",
        }
        missing_templates = [
            template for template in required_templates
            if not any(template in line for line in ordered_source)
        ]
        spawn_commands = sum(
            line.lstrip().startswith("createsprite ") for line in ordered_source)
        if missing_templates or spawn_commands < 14 or move_command_count < 20:
            raise RuntimeError(
                "Blizzard animation fall-through audit failed: "
                f"missing={missing_templates}, sprites={spawn_commands}, "
                f"commands={move_command_count}")

if len(program_command_rows) > 0xFFFF or len(program_argument_values) > 0xFFFF:
    raise RuntimeError(
        f"Animation bytecode exceeds 16-bit offsets: {len(program_command_rows)} commands, "
        f"{len(program_argument_values)} arguments")

unclassified_tasks = [
    name for name, handler in task_handler_audit.items()
    if handler["kind"] == "Pulse"
]
if unclassified_tasks:
    raise RuntimeError(
        "Visual tasks fell through to the legacy generic handler: " +
        ", ".join(sorted(unclassified_tasks)))

# Every visual callback must now be either represented by an exact FireRed
# SpriteTemplate or classified as a concrete procedural family. `Effect`
# without an internal template means the generator would draw the same legacy
# pulse regardless of what the original callback did; fail here instead of
# letting another broken move reach hardware unnoticed.
unhandled_visual_tasks = [
    name for name, handler in task_handler_audit.items()
    if handler["kind"] == "Effect" and not handler["templates"]
]
if unhandled_visual_tasks:
    raise RuntimeError(
        "Procedural visual tasks lack a renderer family: " +
        ", ".join(sorted(unhandled_visual_tasks)))

tagged_handler_assets = [
    str(handler["asset"]) for handler in sprite_handler_audit.values()
    if handler["tag"]
]
if (not tagged_handler_assets or
        len(tagged_handler_assets) != len(set(tagged_handler_assets))):
    duplicates = sorted({
        asset for asset in tagged_handler_assets
        if tagged_handler_assets.count(asset) > 1
    })
    raise RuntimeError(
        "SpriteTemplate exact-cel coverage failed; shared assets=" +
        ", ".join(duplicates))
unhandled_sprite_callbacks = [
    template for template, handler in sprite_handler_audit.items()
    if handler["motion"] == "ProceduralPulse"
]
if unhandled_sprite_callbacks:
    raise RuntimeError(
        "Tagged sprite callbacks lack a renderer family: " +
        ", ".join(sorted(unhandled_sprite_callbacks)))

# These callbacks are exported only as cels for an exact visual-task
# renderer; their source coordinate writes are reproduced by that owning
# task rather than by an independently spawned sprite instance.
TASK_OWNED_STATIC_CALLBACKS = {"AnimIon"}

static_moving_state_machines = [
    f"{template}/{handler['callback']}"
    for template, handler in sprite_handler_audit.items()
    if (handler["stateMovesCoordinates"] and handler["motion"] == "Static" and
        handler["callback"] not in TASK_OWNED_STATIC_CALLBACKS)
]
if static_moving_state_machines:
    raise RuntimeError(
        "Moving FireRed sprite state machines fell through to Static: " +
        ", ".join(sorted(static_moving_state_machines)))

heuristic_sprite_callbacks = [
    f"{template}/{handler['callback']}"
    for template, handler in sprite_handler_audit.items()
    if handler["coverage"] == "HeuristicFallback"
]
if heuristic_sprite_callbacks:
    raise RuntimeError(
        "Sprite callbacks still rely on an unaudited name heuristic: " +
        ", ".join(sorted(heuristic_sprite_callbacks)))

# Follow every reachable branch of each move and prove that it owns at least
# one executable visual. Sound/query-only support commands cannot be mistaken
# for a completed animation.
dedicated_move_ids: set[int] = set()
invisible_moves: list[str] = []
for move_id, move in audit["moves"].items():  # type: ignore[union-attr]
    visible_renderers: set[str] = set()
    for template in move["spriteTemplates"]:
        handler = sprite_handler_audit.get(template)
        if handler and handler["motion"] != "ProceduralPulse":
            visible_renderers.add("Sprite:" + str(handler["motion"]))
    for callback in move["taskCallbacks"]:
        handler = task_handler_audit.get(callback)
        if not handler or handler["kind"] in {"Query", "Control", "Sound"}:
            continue
        if handler.get("motion") == "None" and not handler.get("templates"):
            continue
        visible_renderers.add("Task:" + str(handler.get("motion", handler["kind"])))
    if int(move_id) in dedicated_move_ids:
        visible_renderers.add("DedicatedPlayback")
    move["visualCoverage"] = sorted(visible_renderers)
    if not visible_renderers:
        invisible_moves.append(f"{move_id}:{move['symbol']}")
if invisible_moves:
    raise RuntimeError("Move animations have no executable visual: " +
                       ", ".join(invisible_moves))

program_payload = "\n".join(
    program_rows + program_command_rows + sprite_resource_rows + task_resource_rows)
program_hash = hashlib.sha256(program_payload.encode("utf-8")).hexdigest()[:16]

# The SD compatibility id describes files on the SD card, not firmware-only
# bytecode.  Preserve it when the exact visual-asset specification is
# unchanged; otherwise a harmless script/control-flow correction would make a
# perfectly valid card fail at boot.  The first run after this migration can
# compare the full previous visualAssets object, so existing cards remain
# compatible without a one-off rebuild.
asset_fingerprint = hashlib.sha256(json.dumps(
    visual_assets, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
previous_audit_path = ROOT / ".generated/battle_animation_audit.json"
previous_audit: dict[str, object] = {}
if previous_audit_path.exists():
    try:
        previous_audit = json.loads(previous_audit_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        previous_audit = {}
same_assets = (
    previous_audit.get("assetFingerprint") == asset_fingerprint or
    previous_audit.get("visualAssets") == visual_assets
)
previous_catalog_hash = str(previous_audit.get("catalogHash", ""))
if same_assets and re.fullmatch(r"[0-9a-f]{16}", previous_catalog_hash):
    catalog_hash = previous_catalog_hash
else:
    catalog_hash = asset_fingerprint[:16]
output = ("// Generated from local FireRed battle_anim_scripts.s and exact animation tag tables.\n"
          f'constexpr char kBattleAnimationCatalogId[] = "{catalog_hash}";\n'
          "constexpr BattleAnimationData kBattleAnimations[] = {\n" +
          "\n".join(rows) + "\n};\n"
          "constexpr BattleAnimationSpriteResource kBattleAnimationSpriteResources[] = {\n" +
          "\n".join(sprite_resource_rows) + "\n};\n"
          "constexpr BattleAnimationFrameStep kBattleAnimationFrameSteps[] = {\n" +
          "\n".join(sprite_frame_step_rows) + "\n};\n"
          "constexpr BattleAnimationFrameSequence kBattleAnimationFrameSequences[] = {\n" +
          "\n".join(sprite_frame_sequence_rows) + "\n};\n"
          "constexpr BattleAnimationAffineCommand kBattleAnimationAffineCommands[] = {\n" +
          ("\n".join(sprite_affine_command_rows) or "  {0, 0, 0, 0},") + "\n};\n"
          "constexpr BattleAnimationAffineSequence kBattleAnimationAffineSequences[] = {\n" +
          ("\n".join(sprite_affine_sequence_rows) or "  {0, 0, 255},") + "\n};\n"
          "constexpr BattleAnimationTaskResource kBattleAnimationTaskResources[] = {\n" +
          "\n".join(task_resource_rows) + "\n};\n"
          "constexpr int16_t kBattleAnimationArguments[] = {\n  " +
          ", ".join(str(value) for value in program_argument_values) + "\n};\n"
          "constexpr BattleAnimationCommand kBattleAnimationCommands[] = {\n" +
          "\n".join(program_command_rows) + "\n};\n"
          "constexpr BattleAnimationProgram kBattleAnimationPrograms[] = {\n" +
          "\n".join(program_rows) + "\n};\n")
(ROOT / "src/game/BattleAnimationGenerated.inc").write_text(
    output, encoding="utf-8")

# PlatformIO's dependency scanner does not consistently notice changes to an
# extension-less generated include on Windows. Keep a content stamp for the
# complete firmware bytecode (separate from SD compatibility) in the owning
# .cpp, so a corrected program always recompiles before it is flashed.
owner_path = ROOT / "src/game/BattleAnimationData.cpp"
owner = owner_path.read_text(encoding="utf-8")
stamp = f"// Battle animation data: {program_hash}"
stamp_pattern = r"// Battle animation (?:catalog|data): [0-9a-f]{16}"
if re.search(stamp_pattern, owner):
    stamped_owner = re.sub(stamp_pattern, stamp, owner)
else:
    stamped_owner = owner.rstrip() + f"\n\n{stamp}\n"
if stamped_owner != owner:
    owner_path.write_text(stamped_owner, encoding="utf-8")
audit["catalogHash"] = catalog_hash
audit["assetFingerprint"] = asset_fingerprint
audit["programHash"] = program_hash
audit["implicitFallthroughLabels"] = sum(
    has_physical_fallthrough(label) for label in LABEL_BODIES)
audit["program"] = {
    "commands": len(program_command_rows),
    "arguments": len(program_argument_values),
    "spriteResources": len(sprite_resource_rows),
    "frameSequences": len(sprite_frame_sequence_rows),
    "frameSteps": len(sprite_frame_step_rows),
    "affineSequences": len(sprite_affine_sequence_rows),
    "affineCommands": len(sprite_affine_command_rows),
    "taskResources": len(task_resource_rows),
    "moves": len(program_rows),
}
coverage_counts: dict[str, int] = {}
for handler in task_handler_audit.values():
    renderer = str(handler["renderer"])
    coverage_counts[renderer] = coverage_counts.get(renderer, 0) + 1
audit["taskCoverage"] = coverage_counts
sprite_coverage_counts: dict[str, int] = {}
for handler in sprite_handler_audit.values():
    coverage = str(handler["coverage"])
    sprite_coverage_counts[coverage] = sprite_coverage_counts.get(coverage, 0) + 1
audit["spriteCoverage"] = sprite_coverage_counts
(ROOT / ".generated/battle_animation_audit.json").parent.mkdir(parents=True, exist_ok=True)
(ROOT / ".generated/battle_animation_audit.json").write_text(
    json.dumps(audit, indent=2), encoding="utf-8")
matrix_lines = [
    "# FireRed + expansion battle animation coverage",
    "",
    "Generated from every reachable branch of the original 354 FireRed move "
    "scripts plus the exact expansion overlay selected for Gen-IV and Gen-V moves. "
    "Generation fails if a visual task has no renderer, if an installed sprite "
    "state callback moves but resolves to Static, if any SpriteTemplate still "
    "depends on an unaudited name heuristic, or if a non-dummy frame/affine "
    "stream cannot be parsed. Frame variants, durations and GBA affine commands "
    "are retained rather than flattened into a generic carousel.",
    "",
    "## Sprite callback proof",
    "",
    "| Proof | SpriteTemplates |",
    "|---|---:|",
]
for coverage, count in sorted(sprite_coverage_counts.items()):
    matrix_lines.append(f"| {coverage} | {count} |")
matrix_lines += [
    "",
    "| SpriteTemplate | Callback | Frame seq. / selector | Affine seq. / selector | Lifetime | Installed states | Motion | Travel | Origin | Proof |",
    "|---|---|---|---|---|---|---|---|---|---|",
]
for template, handler in sorted(sprite_handler_audit.items()):
    matrix_lines.append(
        f"| {template} | {handler['callback']} | "
        f"{handler['sequences']} / {handler['frameSelector']} | "
        f"{handler['affineSequences']} / {handler['affineSelector']} | "
        f"{handler['lifetime']} ({handler['lifetimeProof']}) | "
        f"{', '.join(handler['stateCallbacks']) or '-'} | {handler['motion']} | "
        f"{handler['travel']} | {handler['origin']} | {handler['coverage']} |")
matrix_lines += [
    "",
    "## Move coverage",
    "",
    "| ID | Move | Branches | Sprite callbacks | Task callbacks | Retained renderers |",
    "|---:|---|---:|---:|---:|---|",
]
for move_id in sorted(audit["moves"], key=lambda value: int(value)):  # type: ignore[arg-type]
    move = audit["moves"][move_id]  # type: ignore[index]
    matrix_lines.append(
        f"| {move_id} | {move['symbol']} | {move['conditionalBranches']} | "
        f"{len(move['spriteTemplates'])} | {len(move['taskCallbacks'])} | "
        f"{', '.join(move['visualCoverage'])} |")
(ROOT / "docs/BATTLE_ANIMATION_AUDIT.md").write_text(
    "\n".join(matrix_lines) + "\n", encoding="utf-8")
missing = sum(not move["sourceFound"] for move in audit["moves"].values())  # type: ignore[union-attr]
untranslated = sum(len(move["untranslated"]) for move in audit["moves"].values())  # type: ignore[union-attr]
expected_rows = 354 + len(EXPANSION_MOVE_IDS)
if len(rows) != expected_rows or missing or untranslated:
    raise RuntimeError(
        f"Animation coverage failed: {len(rows)} rows, {missing} missing scripts, "
        f"{untranslated} untranslated command groups")
print(f"Generated {len(rows)} FireRed + expansion move animation descriptors from "
      f"{len(visual_assets)} exact visual tags")
