#!/usr/bin/env python3
# =============================================================================
# generate-colored-icons.py — Pre-generate statically colored icon variants
# =============================================================================
# 5WHY (2026-08-01): Every runtime colorization mechanism failed on at least
# one platform (ShaderEffect inline GLSL: no Metal support; MultiEffect:
# QtQuick.Effects absent from iOS aqt; Image.color: property never existed;
# Rectangle overlay: unmasked foggy square).  Decision: NO runtime coloring
# at all.  This script bakes every palette color into a static SVG variant
# at build/commit time.  AppIcon.qml merely SELECTS a file (nearest palette
# color) and expresses alpha via Image.opacity — zero runtime colorization.
#
# v2 (2026-08-10, Living Diagnostics): 4-sentinel color-slot baking.
# Master SVGs use 4 sentinel placeholders that get replaced at generation:
#   #FFFFFF → primary palette color (theme-adaptive, same as v1)
#   #AAAAAA → darken(primary, 30%) (gradient dark-end, same hue)
#   #000000 → semantic accent color (fixed per-icon, from SEMANTIC_ACCENT table)
#   #777777 → soft fill/shadow (fixed, theme-independent)
# This produces dual-color+gradient textured icons from a single master SVG
# set and a single generation pipeline — no second icon directory needed.
#
# Inputs:   src/Common/View/theme/Palette.js   (color roles, both themes)
#           resources/icons/ffffff/*.svg       (master SVGs with 4 sentinel slots)
# Outputs:  resources/icons/<rrggbb>/<name>.svg
#           resources/resources_icons.qrc
#           src/Common/View/widgets/IconColors.js  (list for nearest-match)
#
# Run:      python scripts/generate-colored-icons.py
#           Re-run whenever Palette.js or resources/icons/ffffff/*.svg change.
#           (Enforced by scripts/pre-commit check.)
# =============================================================================
import colorsys
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PALETTE = ROOT / "src/Common/View/theme/Palette.js"
ICONS = ROOT / "resources/icons/ffffff"
OUT = ROOT / "resources/icons"
QRC = ROOT / "resources/resources_icons.qrc"
JS = ROOT / "src/Common/View/widgets/IconColors.js"

# ── Semantic accent colors (fixed per icon, independent of theme) ──────────
# These are the "emphasis" color — the second hue in dual-color icons.
# Network → cyan/blue, Security → green, Warnings → amber,
# Databases → brand colors, System → indigo.
SEMANTIC_ACCENT: dict[str, str] = {
    # ── G4 Remote Host (network/connectivity → cyan/blue) ──
    "ping":                  "#0EA5E9",  # sky-500
    "traceroute":            "#0EA5E9",
    "path-ping":             "#0EA5E9",
    "dns-resolve":           "#06B6D4",  # cyan-500
    "mtu":                   "#0EA5E9",
    "ipv6":                  "#6366F1",  # indigo-500
    # ── G5 Protocol (varied semantics) ──
    "tcp-connect":           "#0EA5E9",  # network
    "certificate":           "#10B981",  # emerald-500 (security)
    "dns-shield":            "#10B981",  # security
    "cloud-shield":          "#10B981",
    "shield-network":        "#10B981",
    "security-headers":      "#10B981",
    "http-headers":          "#0EA5E9",
    "http-timing":           "#F59E0B",  # amber-500 (timing/performance)
    "http-redirect":         "#0EA5E9",
    "http-compression":      "#6366F1",
    "curl-verbose":          "#0EA5E9",
    "url-parse":             "#0EA5E9",
    "banner":                "#6366F1",
    "ftp":                   "#0EA5E9",
    "ssh":                   "#10B981",  # security
    "mail":                  "#F59E0B",
    "telnet":                "#0EA5E9",
    "mysql":                 "#E88D2A",  # MySQL orange
    "postgres":              "#336791",  # PostgreSQL blue
    "redis":                 "#DC382D",  # Redis red
    "mongodb":               "#47A248",  # MongoDB green
    "ldap":                  "#6366F1",
    "mqtt":                  "#6366F1",
    # ── G3 Internet & DNS ──
    "internet-globe":        "#0EA5E9",
    "internet-check":        "#10B981",
    "dns-server":            "#0EA5E9",
    "dns-cache":             "#0EA5E9",
    "geo-location":          "#F59E0B",
    # ── G1 System & Adapters (hardware → indigo/purple) ──
    "network-card":          "#818CF8",  # indigo-400
    "cpu":                   "#818CF8",
    "wifi":                  "#0EA5E9",
    "ethernet":              "#818CF8",
    "dhcp":                  "#F59E0B",
    "ip-config":             "#818CF8",
    "connections":           "#0EA5E9",
    "cellular":              "#0EA5E9",
    # ── G2 Connectivity & Security ──
    "network-profile":       "#818CF8",
    "tcp-settings":          "#0EA5E9",
    "gateway":               "#0EA5E9",
    "route-table":           "#0EA5E9",
    "arp-table":             "#0EA5E9",
    "proxy":                 "#10B981",
    # ── Group icons ──
    "protocol-stack":        "#6366F1",
    "remote-host":           "#0EA5E9",
    # ── Status badges (no accent — monochrome) ──
    "badge-check":           "",  # uses primary only
    "badge-warning":         "",
    "badge-close":           "",
    "badge-skip":            "",
    "badge-error":           "",
    "badge-info":            "",
    "badge-circle":          "",
    "spinner":               "",
}


def darken_hex(hex_color: str, pct: float) -> str:
    """Darken a hex color by reducing lightness in HSL space.

    Args:
        hex_color: '#RRGGBB' string (e.g. '#60C8F8')
        pct: Percentage to darken (0-100, e.g. 30 = reduce L by 30%)

    Returns:
        '#RRGGBB' string of the darkened color.
    """
    if not hex_color or not hex_color.startswith("#") or len(hex_color) != 7:
        return hex_color
    r = int(hex_color[1:3], 16) / 255.0
    g = int(hex_color[3:5], 16) / 255.0
    b = int(hex_color[5:7], 16) / 255.0
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    # 5WHY: very dark primaries (e.g. #0F172A L~0.11) darkened by 30% produce
    # nearly-black output (#080E1B) — the gradient becomes invisible.  Floor
    # the output lightness at 0.05 so the gradient dark-end is at least
    # distinguishable from true black.
    target_l = max(0.05, l * (1.0 - pct / 100.0))
    r2, g2, b2 = colorsys.hls_to_rgb(h, target_l, s)
    # 5WHY: round() not int() — IEEE 754 truncation can shift hex values.
    return f"#{round(r2 * 255):02X}{round(g2 * 255):02X}{round(b2 * 255):02X}"


def palette_hexes() -> list[str]:
    text = PALETTE.read_text(encoding="utf-8")
    hexes = set(m.upper() for m in re.findall(r'"(#[0-9A-Fa-f]{6})"', text))
    hexes.add("#FFFFFF")  # master color, used by "white" callers
    hexes.add("#000000")  # ShadowIcon drop-shadow (alpha via Image.opacity)
    return sorted(hexes)


def main() -> None:
    hexes = palette_hexes()
    # Master SVGs use 4 sentinel color slots.  At minimum #FFFFFF must be
    # present (single-color fallback).  Icons with #AAAAAA / #000000 / #777777
    # slots get dual-color+gradient baking.
    icons = [p for p in sorted(ICONS.glob("*.svg"))
             if "#FFFFFF" in p.read_text(encoding="utf-8").upper()]
    if not icons:
        raise SystemExit("no master icons found in resources/icons/ffffff/")

    # Remove stale color-variant dirs (keep the ffffff master dir plus
    # non-color files like netanalysis.ico / netanalysis.png / app-icon.svg).
    for sub in sorted(OUT.iterdir()):
        if sub.is_dir() and re.fullmatch(r"[0-9a-f]{6}", sub.name) and sub.name != "ffffff":
            shutil.rmtree(sub)

    # Cache master SVG bodies once to avoid O(colors) redundant disk reads
    # (~1900 avoidable reads per 60 icons × 32 colors on slow I/O / CI).
    cached_bodies: dict[Path, str] = {}
    unmapped_accent: set[str] = set()
    for svg in icons:
        body = svg.read_text(encoding="utf-8")
        cached_bodies[svg] = body
        # 5WHY: if a master SVG uses #000000 but has no SEMANTIC_ACCENT entry,
        # #000000 stays as literal black in ALL generated variants — silent.
        if "#000000" in body.upper() and not SEMANTIC_ACCENT.get(svg.stem, ""):
            unmapped_accent.add(svg.stem)
    if unmapped_accent:
        print(f"WARNING: {len(unmapped_accent)} icon(s) use #000000 sentinel "
              f"but have no SEMANTIC_ACCENT mapping — #000000 will stay as "
              f"literal black: {', '.join(sorted(unmapped_accent))}")

    qrc_entries = []
    for hx in hexes:
        if hx == "#FFFFFF":
            sub = ICONS
        else:
            sub = OUT / hx[1:].lower()
            sub.mkdir(parents=True)
            dark = darken_hex(hx, 30)
            for svg in icons:
                body = cached_bodies[svg]  # cached reads (avoid per-color I/O)
                # ── 4-sentinel color-slot baking ────────────────────────────
                # #FFFFFF → primary palette color (theme-adaptive)
                colored = body.replace("#FFFFFF", hx).replace("#ffffff", hx)
                # #AAAAAA → darken(primary, 30%) (gradient dark-end)
                colored = colored.replace("#AAAAAA", dark).replace("#aaaaaa", dark)
                # #000000 → semantic accent (fixed per icon); lowercase too
                accent = SEMANTIC_ACCENT.get(svg.stem, "")
                if accent:
                    colored = colored.replace("#000000", accent)
                    colored = colored.replace("#000000", accent)  # lowercase hex variant
                # #777777 → soft fill (fixed, theme-independent)
                colored = colored.replace("#777777", "#64748B")
                colored = colored.replace("#777777", "#64748B")  # lowercase hex variant
                out_file = sub / svg.name
                out_file.write_text(colored, encoding="utf-8", newline="\n")
        for svg in icons:
            qrc_entries.append(f"icons/{sub.name}/{svg.name}")

    qrc = ['<?xml version="1.0" encoding="utf-8"?>', "<RCC>", '    <qresource prefix="/">']
    qrc += [f"        <file>{e}</file>" for e in qrc_entries]
    qrc += ["    </qresource>", "</RCC>", ""]
    QRC.write_text("\n".join(qrc), encoding="utf-8", newline="\n")

    js = [
        "// AUTO-GENERATED by scripts/generate-colored-icons.py — DO NOT EDIT.",
        "// List of pre-generated icon colors for nearest-match selection.",
        ".pragma library",
        "var hexes = [",
    ]
    js += [f'    "{h}",' for h in hexes]
    js += ["];", ""]
    JS.write_text("\n".join(js), encoding="utf-8", newline="\n")

    accented = sum(1 for s in icons if SEMANTIC_ACCENT.get(s.stem, ""))
    print(f"generated {len(hexes)} colors x {len(icons)} icons = "
          f"{len(qrc_entries)} files ({accented} dual-color)")


if __name__ == "__main__":
    main()
