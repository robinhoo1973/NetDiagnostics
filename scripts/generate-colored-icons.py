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
#
# v3 (2026-08-13, M1 45 专属图标): nd-diag-* 主形同现有 4-sentinel 管线烘焙；
#   DIAG_ACCENT 逐项辅色对（dark/light）将 masters-45/nd-diag-*-a.svg 的
#   #666666 sentinel 烘焙为 2 色/图标并合并进 qrc（见 bake_diag_accents）。
#   #101010 sentinel → SECOND_ACCENT 固定第二强调色（多色复刻图标用，如网关红色箭头）。
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
MASTERS_ACCENT = ROOT / "resources/icons/masters-45"

# ── 45 专属图标：逐项辅色对（dark, light）——值=01 §8 各节"辅"列；
#   灰值（#2/#6/#13/#30/#42）保留（W9）。同时作为 #000000 sentinel 的语义强调色。──
DIAG_ACCENT: dict[str, tuple[str, str]] = {
    "nd-diag-g1-network-adapters": ("#68E5F4", "#06B6D4"),
    "nd-diag-g1-nic-advanced":      ("#94A3B8", "#475569"),
    "nd-diag-g1-wifi-info":         ("#60C8F8", "#0EA5E9"),
    "nd-diag-g1-wired":             ("#68E5F4", "#06B6D4"),
    "nd-diag-g1-dhcp":              ("#F59E0B", "#EA580C"),
    "nd-diag-g1-ip-config":         ("#94A3B8", "#475569"),
    "nd-diag-g1-active-connections":("#68E5F4", "#06B6D4"),
    "nd-diag-g1-cellular":          ("#818CF8", "#6366F1"),
    "nd-diag-g2-network-profile":   ("#60C8F8", "#0EA5E9"),
    "nd-diag-g2-tcp-settings":      ("#68E5F4", "#06B6D4"),
    "nd-diag-g2-gateway":           ("#38BDF8", "#0EA5E9"),
    "nd-diag-g2-routing-table":     ("#A5B4FC", "#2563EB"),
    "nd-diag-g2-arp-table":         ("#94A3B8", "#475569"),
    "nd-diag-g2-proxy":             ("#818CF8", "#6366F1"),
    "nd-diag-g3-dns-servers":       ("#38BDF8", "#0EA5E9"),
    "nd-diag-g3-dns-cache":         ("#F59E0B", "#EA580C"),
    "nd-diag-g3-dns-integrity":     ("#68E5F4", "#06B6D4"),
    "nd-diag-g3-geoip":             ("#4ADE80", "#10B981"),
    "nd-diag-g3-internet":          ("#68E5F4", "#06B6D4"),
    "nd-diag-g4-dns-resolution":    ("#60C8F8", "#0EA5E9"),
    "nd-diag-g4-ping":              ("#4ADE80", "#10B981"),
    "nd-diag-g4-traceroute":        ("#A5B4FC", "#2563EB"),
    "nd-diag-g4-pathping":          ("#F59E0B", "#EA580C"),
    "nd-diag-g4-mtu":               ("#F59E0B", "#EA580C"),
    "nd-diag-g4-ipv6":              ("#818CF8", "#6366F1"),
    "nd-diag-g5-url-parsing":       ("#68E5F4", "#06B6D4"),
    "nd-diag-g5-tcp-connect":       ("#68E5F4", "#06B6D4"),
    "nd-diag-g5-service-banner":    ("#60C8F8", "#0EA5E9"),
    "nd-diag-g5-curl-verbose":      ("#68E5F4", "#06B6D4"),
    "nd-diag-g5-http-headers":      ("#94A3B8", "#475569"),
    "nd-diag-g5-security-headers":  ("#60C8F8", "#0EA5E9"),
    "nd-diag-g5-ssl-certificate":   ("#68E5F4", "#06B6D4"),
    "nd-diag-g5-http-redirect":     ("#818CF8", "#6366F1"),
    "nd-diag-g5-http-compression":  ("#68E5F4", "#06B6D4"),
    "nd-diag-g5-http-timing":       ("#818CF8", "#6366F1"),
    "nd-diag-g5-ftp":               ("#F59E0B", "#EA580C"),
    "nd-diag-g5-ssh":               ("#4ADE80", "#10B981"),
    "nd-diag-g5-email":             ("#818CF8", "#6366F1"),
    "nd-diag-g5-telnet":            ("#60C8F8", "#0EA5E9"),
    "nd-diag-g5-mysql":             ("#FBBF24", "#D97706"),
    "nd-diag-g5-postgres":          ("#60C8F8", "#0EA5E9"),
    "nd-diag-g5-redis":             ("#F87171", "#DC2626"),
    "nd-diag-g5-mongodb":           ("#4ADE80", "#10B981"),
    "nd-diag-g5-ldap":              ("#60C8F8", "#0EA5E9"),
    "nd-diag-g5-mqtt":              ("#818CF8", "#6366F1"),
}

# ── 第二强调色（固定色，逐图标；#101010 sentinel）────────────────────────
# 用于 VTracer 多色复刻图标中除主强调色外的第二种色彩（如网关的红色箭头）。
SECOND_ACCENT: dict[str, str] = {
    "nd-diag-g2-gateway": "#E03040",
}

# ── 多色固定色（逐图标；#B0000n sentinel → 第 n 个固定色）─────────────────
# 用于一根图标内多种独立固定色彩（如蜂窝信号四根柱各一色）。
FIXED_COLORS: dict[str, list[str]] = {
    "nd-diag-g1-cellular": ["#F87171", "#FBBF24", "#4ADE80", "#38BDF8"],
    "nd-diag-g3-geoip": ["#F43F5E"],
    "nd-diag-g3-internet": ["#FD3551", "#4C8FFF", "#FFE824"],
    # 终端三协议（用户 ASCII 规格）：屏幕 #0F172A + 标题栏三圆点红黄绿
    "nd-diag-g5-ssh": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80"],
    "nd-diag-g5-ftp": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80"],
    "nd-diag-g5-telnet": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80"],
    # IPv6 显示器：电源点+底座 → 浅腚（与 DIAG_ACCENT 浅色端一致）
    "nd-diag-g4-ipv6": ["#818CF8"],
    # Proxy 地球线框：蓝色循环箭头 + 黄色闪电（忠实参考图色）
    "nd-diag-g2-proxy": ["#5CAEFF", "#FFCC5E"],
    # DB 四联：背景与 yxdb 参考图一致（文件体/折边带/折痕阴影/底部带固定色）
    "nd-diag-g5-mysql": ["#3AAEFC", "#37AAF9", "#2E9FF1", "#C8D8ED"],
    "nd-diag-g5-postgres": ["#3AAEFC", "#37AAF9", "#2E9FF1", "#C8D8ED"],
    "nd-diag-g5-redis": ["#3AAEFC", "#37AAF9", "#2E9FF1", "#C8D8ED"],
    "nd-diag-g5-mongodb": ["#3AAEFC", "#37AAF9", "#2E9FF1", "#C8D8ED"],
}

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
    "redirect":              "#0EA5E9",
    "compression":           "#6366F1",
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
    "devices":               "#06B6D4",
    "activity":              "#10B981",
}

# 45 专属图标以 light 辅色作为 #000000 sentinel 的语义强调色（合并查表）。
ACCENT: dict[str, str] = {
    **SEMANTIC_ACCENT,
    **{stem: pair[1] for stem, pair in DIAG_ACCENT.items()},
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


def bake_diag_accents(qrc_entries: list[str]) -> None:
    """辅形 #666666 → 逐项辅色（dark/light 各 1 文件/图标）；条目并入 qrc。

    W2（终审）：masters-45/ 不在主扫描目录内，必须在此显式烘焙并合并 qrc 条目。
    """
    if not MASTERS_ACCENT.is_dir():
        return
    for svg in sorted(MASTERS_ACCENT.glob("*-a.svg")):
        stem = svg.stem[:-2]   # 去尾缀 "-a"
        pair = DIAG_ACCENT.get(stem)
        if not pair:
            print(f"WARNING: no DIAG_ACCENT for {stem} — skipping aux bake")
            continue
        body = svg.read_text(encoding="utf-8")
        for hx in pair:
            sub = OUT / hx[1:].lower()
            sub.mkdir(parents=True, exist_ok=True)
            out = sub / svg.name
            out.write_text(body.replace("#666666", hx), encoding="utf-8", newline="\n")
            qrc_entries.append(f"icons/{sub.name}/{svg.name}")


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
        if "#000000" in body.upper() and not ACCENT.get(svg.stem, ""):
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
                # #000000 → semantic accent (fixed per icon)
                accent = ACCENT.get(svg.stem, "")
                if accent:
                    colored = colored.replace("#000000", accent)
                # #101010 → second semantic accent (fixed per icon)
                accent2 = SECOND_ACCENT.get(svg.stem, "")
                if accent2:
                    colored = colored.replace("#101010", accent2)
                # #B0000n → n-th fixed color (per icon, FIXED_COLORS)
                for i, c in enumerate(FIXED_COLORS.get(svg.stem, []), 1):
                    colored = colored.replace(f"#B0000{i}", c)
                # #777777 → soft fill (fixed, theme-independent)
                colored = colored.replace("#777777", "#64748B")
                out_file = sub / svg.name
                out_file.write_text(colored, encoding="utf-8", newline="\n")
        for svg in icons:
            qrc_entries.append(f"icons/{sub.name}/{svg.name}")

    # ── 辅形（masters-45/nd-diag-*-a.svg，#666666 → 逐项辅色对 dark/light）──
    bake_diag_accents(qrc_entries)

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

    accented = sum(1 for s in icons if ACCENT.get(s.stem, ""))
    print(f"generated {len(hexes)} colors x {len(icons)} icons = "
          f"{len(qrc_entries)} files ({accented} dual-color)")


if __name__ == "__main__":
    main()
