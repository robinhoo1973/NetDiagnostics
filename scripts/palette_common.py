#!/usr/bin/env python3
# =============================================================================
# palette_common.py — Shared Palette.js parsing + WCAG color math
# =============================================================================
# 5WHY (2026-08-17 code review): three scripts each re-implemented their own
# regex parser for Palette.js (parse_palette ×2 + theme_hexes) and one more
# duplicated a weaker, non-gamma-corrected luminance.  A new syntax form in
# Palette.js had to be fixed independently in every copy — the one that was
# missed silently produced wrong output (stale WCAG report / wrong icon
# fixed-color table) with no error.  Single shared module = single fix point.
#
# Used by:
#   generate-appcolors.py      (ROLES → AppColors.h)
#   audit-palette-contrast.py  (WCAG report)
#   generate-colored-icons.py  (per-theme fixed colors + tint luminance)
#
# Invocation contract: imported as `python3 scripts/<caller>.py` (the script
# directory is on sys.path[0] — same-dir import).  The module form
# `python3 -m scripts.<caller>` is NOT supported (scripts/ is not a package).
# =============================================================================
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PALETTE_JS = ROOT / "src/Common/View/theme/Palette.js"


def read_palette() -> str:
    """Read Palette.js text once; callers parse it and reuse the result."""
    return PALETTE_JS.read_text(encoding="utf-8")


def parse_palette(text: str) -> dict:
    """Parse Palette.js into {"Dark": {role: "#RRGGBB"}, "Light": {...}}.

    Only `role: "#hex"` color entries are parsed — non-color tokens
    (iconPadAlpha, groupHues arrays) and reference forms (`x: Dark.y`)
    are intentionally skipped.
    """
    palettes: dict[str, dict] = {}
    cur = None
    for line in text.splitlines():
        s = line.strip()
        m = re.match(r"^var (Dark|Light) = \{", s)
        if m:
            cur = m.group(1)
            palettes[cur] = {}
            continue
        if cur is not None and s == "};":
            cur = None
            continue
        if cur is not None:
            kv = re.match(r'^([A-Za-z0-9_]+):\s*"(#[0-9A-Fa-f]{6,8})"', s)
            if kv:
                palettes[cur][kv.group(1)] = kv.group(2).upper()
    return palettes


def theme_hexes(text: str) -> dict:
    """All quoted 6-digit hexes per theme block (roles AND arrays).

    Used by generate-colored-icons.py to pick the light/dark fixed-color
    table: a variant hex that appears only in the Light block bakes with
    the light table; a hex in both blocks (or in arrays) bakes with the
    dark table.  AppIcon's nearest-match resolves the active theme at
    runtime, so the split only decides which fixed-color table is baked.
    """
    hexes: dict[str, set[str]] = {"Dark": set(), "Light": set()}
    cur = None
    for line in text.splitlines():
        s = line.strip()
        m = re.match(r"^var (Dark|Light) = \{", s)
        if m:
            cur = m.group(1)
            continue
        if cur is not None and s == "};":
            cur = None
            continue
        if cur is not None:
            for hx in re.findall(r'"(#[0-9A-Fa-f]{6})"', line):
                hexes[cur].add(hx.upper())
    return hexes


def all_keys(text: str) -> set[str]:
    """Every token key declared in the Dark/Light blocks.

    Unlike parse_palette (color entries only), this includes non-color tokens
    (iconPadAlpha, groupHues) and derived getters (terminalBg) — used by the
    pre-commit QML-reference check so the token-key grammar lives in exactly
    one place (5WHY simplify 2026-08-17: the check previously re-parsed
    Palette.js with its own 4-space-indent-anchored regex — a 4th grammar).
    """
    keys: set[str] = set()
    cur = False
    for line in text.splitlines():
        s = line.strip()
        m = re.match(r"^var (Dark|Light) = \{", s)
        if m:
            cur = True
            continue
        if cur and s == "};":
            cur = False
            continue
        if cur:
            kv = re.match(r"^([A-Za-z0-9_]+):", s)
            if kv:
                keys.add(kv.group(1))
            gm = re.match(r"^get ([A-Za-z0-9_]+)\(\)", s)
            if gm:
                keys.add(gm.group(1))
    return keys


def all_hexes(text: str) -> list[str]:
    """Sorted set of every 6-digit hex in Palette.js + #FFFFFF + #000000."""
    hexes = set(m.upper() for m in re.findall(r'"(#[0-9A-Fa-f]{6})"', text))
    hexes.add("#FFFFFF")  # master color, used by "white" callers
    hexes.add("#000000")  # ShadowIcon drop-shadow (alpha via Image.opacity)
    return sorted(hexes)


def srgb_to_lin(c: float) -> float:
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def luminance(hex_color: str) -> float:
    """WCAG 2.1 relative luminance (gamma-corrected sRGB)."""
    if len(hex_color) < 7:
        raise ValueError(f"bad hex: {hex_color}")
    r = int(hex_color[1:3], 16)
    g = int(hex_color[3:5], 16)
    b = int(hex_color[5:7], 16)
    return 0.2126 * srgb_to_lin(r) + 0.7152 * srgb_to_lin(g) + 0.0722 * srgb_to_lin(b)


def contrast(fg: str, bg: str) -> float:
    """WCAG 2.1 contrast ratio between two #RRGGBB colors."""
    l1, l2 = luminance(fg), luminance(bg)
    hi, lo = max(l1, l2), min(l1, l2)
    return (hi + 0.05) / (lo + 0.05)
