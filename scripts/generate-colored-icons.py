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
# 5WHY (review 2026-08-17): future-annotations 使全部注解惰性化——本文件在
# 3.9 及更早的 Python 上原会因 PEP 604（Path | None）在 def 期求值而崩溃
# （macOS 自带 python3 = 3.9）。
from __future__ import annotations

import argparse
import colorsys
import re
import shutil
import sys
import tempfile
from pathlib import Path

from palette_common import all_hexes, contrast, luminance, parse_palette, read_palette, theme_hexes

ROOT = Path(__file__).resolve().parent.parent
PALETTE = ROOT / "src/Common/View/theme/Palette.js"
ICONS = ROOT / "resources/icons/ffffff"
OUT = ROOT / "resources/icons"
QRC = ROOT / "resources/resources_icons.qrc"
JS = ROOT / "src/Common/View/widgets/IconColors.js"
TINTS_JS = ROOT / "src/Common/View/widgets/IconTints.js"
MASTERS_ACCENT = ROOT / "resources/icons/masters-45"

# ── 45 专属图标：逐项辅色对（dark, light）——值=01 §8 各节"辅"列；
#   灰值（#2/#6/#13/#30/#42）保留（W9）。同时作为 #000000 sentinel 的语义强调色。
#   5WHY (review 2026-08-17): 镜像调色板角色的条目改为 "Theme.role" 引用，
#   生成期解析（调色板重调自动传播）；非角色值为真实美术规范保留字面量。──
DIAG_ACCENT: dict[str, tuple[str, str]] = {
    "nd-diag-g1-network-adapters": ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g1-nic-advanced":      ("Dark.onSurfaceVariant", "Light.onSurfaceVariant"),
    "nd-diag-g1-wifi-info":         ("Dark.primary", "Light.primary"),
    "nd-diag-g1-wired":             ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g1-dhcp":              ("Dark.warning", "Light.warning"),
    "nd-diag-g1-ip-config":         ("Dark.onSurfaceVariant", "Light.onSurfaceVariant"),
    "nd-diag-g1-active-connections":("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g1-cellular":          ("Dark.secondary", "Light.secondary"),
    "nd-diag-g2-network-profile":   ("Dark.primary", "Light.primary"),
    "nd-diag-g2-tcp-settings":      ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g2-gateway":           ("#38BDF8", "#0EA5E9"),
    "nd-diag-g2-routing-table":     ("Dark.info", "Light.info"),
    "nd-diag-g2-arp-table":         ("Dark.onSurfaceVariant", "Light.onSurfaceVariant"),
    "nd-diag-g2-proxy":             ("Dark.secondary", "Light.secondary"),
    "nd-diag-g3-dns-servers":       ("#38BDF8", "#0EA5E9"),
    "nd-diag-g3-dns-cache":         ("Dark.warning", "Light.warning"),
    "nd-diag-g3-dns-integrity":     ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g3-geoip":             ("Dark.success", "Light.success"),
    "nd-diag-g3-internet":          ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g4-dns-resolution":    ("Dark.primary", "Light.primary"),
    "nd-diag-g4-ping":              ("Dark.success", "Light.success"),
    "nd-diag-g4-traceroute":        ("Dark.info", "Light.info"),
    "nd-diag-g4-pathping":          ("Dark.warning", "Light.warning"),
    "nd-diag-g4-mtu":               ("Dark.warning", "Light.warning"),
    "nd-diag-g4-ipv6":              ("Dark.secondary", "Light.secondary"),
    "nd-diag-g5-url-parsing":       ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g5-tcp-connect":       ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g5-service-banner":    ("Dark.primary", "Light.primary"),
    "nd-diag-g5-curl-verbose":      ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g5-http-headers":      ("Dark.onSurfaceVariant", "Light.onSurfaceVariant"),
    "nd-diag-g5-security-headers":  ("Dark.primary", "Light.primary"),
    "nd-diag-g5-ssl-certificate":   ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g5-http-redirect":     ("Dark.secondary", "Light.secondary"),
    "nd-diag-g5-http-compression":  ("Dark.tertiary", "Light.tertiary"),
    "nd-diag-g5-http-timing":       ("Dark.secondary", "Light.secondary"),
    "nd-diag-g5-ftp":               ("Dark.warning", "Light.warning"),
    "nd-diag-g5-ssh":               ("Dark.success", "Light.success"),
    "nd-diag-g5-email":             ("Dark.secondary", "Light.secondary"),
    "nd-diag-g5-telnet":            ("Dark.primary", "Light.primary"),
    "nd-diag-g5-mysql":             ("#FBBF24", "#D97706"),
    "nd-diag-g5-postgres":          ("Dark.primary", "Light.primary"),
    "nd-diag-g5-redis":             ("Dark.fail", "Light.fail"),
    "nd-diag-g5-mongodb":           ("Dark.success", "Light.success"),
    "nd-diag-g5-ldap":              ("Dark.primary", "Light.primary"),
    "nd-diag-g5-mqtt":              ("Dark.secondary", "Light.secondary"),
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
    # 终端三协议（用户 ASCII 规格）：屏幕 #1E293B（与 dark 面区分）+ 标题栏三圆点红黄绿
    "nd-diag-g5-ssh": ["#1E293B", "#F87171", "#FBBF24", "#4ADE80"],
    "nd-diag-g5-ftp": ["#1E293B", "#F87171", "#FBBF24", "#4ADE80"],
    "nd-diag-g5-telnet": ["#1E293B", "#F87171", "#FBBF24", "#4ADE80"],
    # IPv6 显示器：电源点+底座 → 浅腚（与 DIAG_ACCENT 浅色端一致）
    "nd-diag-g4-ipv6": ["#818CF8"],
    # Proxy 地球线框：蓝色循环箭头 + 黄色闪电（忠实参考图色）
    "nd-diag-g2-proxy": ["#5CAEFF", "#FFCC5E"],
    # Gateway：圆 #38BDF8 + 白箭头（dark）；light 双套见 FIXED_COLORS_LIGHT
    "nd-diag-g2-gateway": ["#38BDF8", "#FFFFFF"],
    # Network Profile：blankfile 背景（页面 #007CC9 + 折角 #0094F5）
    "nd-diag-g2-network-profile": ["#007CC9", "#0094F5"],
    # MQTT：blankfile 背景（页面 #007CC9 + 折角 #0094F5）+ 高亮绿色符号
    "nd-diag-g5-mqtt": ["#007CC9", "#0094F5", "#4ADE80"],
    # DB 四联：blankdbfile 1:1 复刻（页面 #007CC9 + 折角 #0094F5 + 深核心 #003E6B/中间调 #0068AA/浅过渡 #007BC8）+ 白描边/白字
    "nd-diag-g5-mysql": ["#007CC9", "#0094F5", "#FFFFFF", "#003E6B", "#0068AA", "#007BC8"],
    "nd-diag-g5-postgres": ["#007CC9", "#0094F5", "#FFFFFF", "#003E6B", "#0068AA", "#007BC8"],
    "nd-diag-g5-redis": ["#007CC9", "#0094F5", "#FFFFFF", "#003E6B", "#0068AA", "#007BC8"],
    "nd-diag-g5-mongodb": ["#007CC9", "#0094F5", "#FFFFFF", "#003E6B", "#0068AA", "#007BC8"],
    # HTTP 六图标：背景 = 1:1 复刻 blankfile.png（页面 #007CC9 + 折角 #0094F5），内部图形保留主题色
    "nd-diag-g5-http-timing": ["#007CC9", "#0094F5", "#FBBF24"],
    "nd-diag-g5-http-redirect": ["#007CC9", "#0094F5"],
    "nd-diag-g5-ssl-certificate": ["#007CC9", "#0094F5", "#FBBF24", "#4ADE80"],
    "nd-diag-g5-security-headers": ["#007CC9", "#0094F5"],
    "nd-diag-g5-http-headers": ["#007CC9", "#0094F5"],
    "nd-diag-g5-curl-verbose": ["#007CC9", "#0094F5"],
    "nd-diag-g5-http-compression": ["#007CC9", "#0094F5"],
}

# ── 浅色主题固定色（#B0000n → 第 n 个固定色，仅 light 变体使用）────────────────
# 双套规则：固定色按主题分 dark/light，烘焙时按变体所属主题选择；
# 未列出的图标浅色主题沿用 FIXED_COLORS（dark 值）。
FIXED_COLORS_LIGHT: dict[str, list[str]] = {
    # A 类：页面双套（dark #007CC9 / light #0094F5，折角 #37AAF9）
    "nd-diag-g2-network-profile": ["#0094F5", "#37AAF9"],
    "nd-diag-g5-mqtt": ["#0094F5", "#37AAF9", "#22C55E"],
    "nd-diag-g5-http-timing": ["#0094F5", "#37AAF9", "#F59E0B"],
    "nd-diag-g5-http-redirect": ["#0094F5", "#37AAF9"],
    "nd-diag-g5-ssl-certificate": ["#0094F5", "#37AAF9", "#F59E0B", "#10B981"],
    "nd-diag-g5-security-headers": ["#0094F5", "#37AAF9"],
    "nd-diag-g5-http-headers": ["#0094F5", "#37AAF9"],
    "nd-diag-g5-curl-verbose": ["#0094F5", "#37AAF9"],
    "nd-diag-g5-http-compression": ["#0094F5", "#37AAF9"],
    # DB 四联 light：页面浅蓝 + 深色细节（白描边/白字 → 深 #0F172A，圆柱加深）
    "nd-diag-g5-mysql": ["#0094F5", "#37AAF9", "#0F172A", "#014A75", "#0369A1", "#0284C7"],
    "nd-diag-g5-postgres": ["#0094F5", "#37AAF9", "#0F172A", "#014A75", "#0369A1", "#0284C7"],
    "nd-diag-g5-redis": ["#0094F5", "#37AAF9", "#0F172A", "#014A75", "#0369A1", "#0284C7"],
    "nd-diag-g5-mongodb": ["#0094F5", "#37AAF9", "#0F172A", "#014A75", "#0369A1", "#0284C7"],
    # B 类：终端三协议 light 屏幕保持深黑；proxy/ipv6/cellular/internet light 加深
    "nd-diag-g5-ssh": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80"],
    "nd-diag-g5-ftp": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80"],
    "nd-diag-g5-telnet": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80"],
    "nd-diag-g2-proxy": ["#1D4ED8", "#F59E0B"],
    "nd-diag-g4-ipv6": ["#6366F1"],
    "nd-diag-g1-cellular": ["#DC2626", "#D97706", "#10B981", "#0EA5E9"],
    "nd-diag-g3-internet": ["#E11D48", "#2563EB", "#F59E0B"],
    "nd-diag-g2-gateway": ["#0EA5E9", "#FFFFFF"],
}

# ── Semantic accent colors (fixed per icon, independent of theme) ──────────
# These are the "emphasis" color — the second hue in dual-color icons.
# Network → cyan/blue, Security → green, Warnings → amber,
# Databases → brand colors, System → indigo.
SEMANTIC_ACCENT: dict[str, str] = {
    # ── 引用审计（2026-08-17）：仅保留 QML/C++ 实际引用的 UI 图标；
    #    46 个旧版未引用图标已移至 review/icons/deleted/，其条目一并清除。──
    # ── G5 Protocol（mail 在 SchemeSelector 分组目录使用）──
    "mail":                  "#F59E0B",
    # ── G3 Internet & DNS ──
    "internet-globe":        "#0EA5E9",
    # ── G1 System & Adapters（hardware → indigo/purple）──
    "network-card":          "#818CF8",  # indigo-400
    # ── Group icons ──
    "protocol-stack":        "#6366F1",
    "remote-host":           "#0EA5E9",
    "shield-network":        "#10B981",
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


def _resolve_role_ref(value: str, palettes: dict) -> str:
    """Resolve "Theme.role" references in the accent tables at generation time.

    5WHY (review 2026-08-17): ~20 DIAG_ACCENT 条目逐字复制了调色板角色值
    （primary/tertiary/secondary/success/onSurfaceVariant）——调色板重调时
    图标强调色静默脱节。角色引用在生成期解析，非角色值是真实美术规范。
    """
    if value.startswith("Dark.") or value.startswith("Light."):
        theme, key = value.split(".", 1)
        if theme not in palettes or key not in palettes[theme]:
            raise SystemExit(f"unknown role reference \"{value}\" — fix the accent table")
        return palettes[theme][key]
    return value


def _composite(hex_color: str, bg: str, alpha: float) -> str:
    """Straight-line alpha blend of hex_color over bg (decorative pad math)."""
    r = int(hex_color[1:3], 16) * alpha + int(bg[1:3], 16) * (1.0 - alpha)
    g = int(hex_color[3:5], 16) * alpha + int(bg[3:5], 16) * (1.0 - alpha)
    b = int(hex_color[5:7], 16) * alpha + int(bg[5:7], 16) * (1.0 - alpha)
    return f"#{round(r):02X}{round(g):02X}{round(b):02X}"


def _pad_border_contrasts(tint: str, palettes: dict) -> tuple[float, float]:
    """(dark, light) WCAG contrast of the 0.22-alpha border composite vs card."""
    out = []
    for theme in ("Dark", "Light"):
        bg = palettes[theme]["surfaceContainerLow"]
        out.append(contrast(_composite(tint, bg, 0.22), bg))
    return tuple(out)


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


def bake_diag_accents(qrc_entries: list[str], out: Path = OUT,
                      palettes: dict | None = None) -> None:
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
            if palettes is not None:
                hx = _resolve_role_ref(hx, palettes)
            sub = out / hx[1:].lower()
            sub.mkdir(parents=True, exist_ok=True)
            out_file = sub / svg.name
            out_file.write_text(body.replace("#666666", hx), encoding="utf-8", newline="\n")
            qrc_entries.append(f"icons/{sub.name}/{svg.name}")


def write_icon_tints(palettes: dict, tints_js_path: Path = TINTS_JS) -> None:
    """逐图标常显主导色（全彩常显方案的瓦片光晕垫 tint）。

    规则：优先取 FIXED_COLORS 中第一个非深色固定色（页面/符号主色）；
    若首色过暗（如终端黑屏）则回退 DIAG_ACCENT 深色端。

    5WHY (2026-08-17 review): the dark-screen gate used a non-gamma-corrected
    linear luminance — #1E293B scored 0.157 ≥ 0.15 and became the tint for
    ssh/ftp/telnet tiles, rendering the glow pad invisible (1:1 against the
    same #1E293B card).  The WCAG gamma-corrected luminance (shared with the
    contrast audit) scores #1E293B ≈ 0.022, so the documented DIAG_ACCENT
    fallback fires again.  The tintFor fallback was also a hardcoded
    "#60C8F8" literal — now read from Palette.js Dark.primary so a primary
    retune propagates at regeneration time.
    """
    tints: dict[str, str] = {}
    for stem, pair in DIAG_ACCENT.items():
        chosen = _resolve_role_ref(pair[0], palettes)
        fcs = FIXED_COLORS.get(stem, [])
        # 5WHY (review 2026-08-17): 0.15 亮度门只挡住近黑屏幕色——#007CC9
        # （L=0.186）过关但 0.22 边框合成后对卡片仅 1.26:1，13 个 DB/HTTP
        # 瓦片的光晕垫不可见。改为合成对比度门（两主题均 ≥1.3）。
        if fcs and luminance(fcs[0]) >= 0.15:
            cd, cl = _pad_border_contrasts(fcs[0], palettes)
            if cd >= 1.3 and cl >= 1.3:
                chosen = fcs[0]
        tints[stem] = chosen
    if "primary" not in palettes.get("Dark", {}):
        raise SystemExit("Palette.js is missing Dark.primary — fix Palette.js "
                         "before regenerating icons")
    fallback = palettes["Dark"]["primary"]
    lines = [
        "// AUTO-GENERATED by scripts/generate-colored-icons.py — DO NOT EDIT.",
        "// Per-icon dominant tint for the tile glow pad (full-color always-on display).",
        ".pragma library",
        "var tints = {",
    ]
    lines += [f'    "{stem}": "{c}",' for stem, c in sorted(tints.items())]
    lines += [
        "};",
        "",
        "function tintFor(name) {",
        f'    return tints[name] || "{fallback}";',
        "}",
        "",
    ]
    tints_js_path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def _resolve_outputs(out_base: Path | None):
    """Output paths — redirected under out_base in --check mode."""
    if out_base is None:
        return OUT, QRC, JS, TINTS_JS, ICONS
    return (
        out_base / "resources/icons",
        out_base / "resources/resources_icons.qrc",
        out_base / "src/Common/View/widgets/IconColors.js",
        out_base / "src/Common/View/widgets/IconTints.js",
        out_base / "resources/icons/ffffff",
    )


def generate(out_base: Path | None = None) -> None:
    """Full regeneration (icons + qrc + IconColors.js + IconTints.js).

    out_base=None writes the real outputs; a redirect (--check) writes into
    a mirror tree for drift comparison.
    """
    out, qrc_path, js_path, tints_path, master_out = _resolve_outputs(out_base)
    # Ensure output roots exist (--check mode starts from an empty mirror).
    out.mkdir(parents=True, exist_ok=True)
    qrc_path.parent.mkdir(parents=True, exist_ok=True)
    js_path.parent.mkdir(parents=True, exist_ok=True)
    tints_path.parent.mkdir(parents=True, exist_ok=True)
    # Single Palette.js read; every parser works on the same text
    # (5WHY review 2026-08-17: the old code re-read and re-parsed the file
    # three times per run with three divergent regex parsers).
    text = read_palette()
    hexes = all_hexes(text)
    themed_hexes = theme_hexes(text)
    dark_hexes, light_hexes = themed_hexes["Dark"], themed_hexes["Light"]
    palettes = parse_palette(text)

    # Master SVGs use 4 sentinel color slots.  At minimum #FFFFFF must be
    # present (single-color fallback).  Icons with #AAAAAA / #000000 / #777777
    # slots get dual-color+gradient baking.
    icons = [p for p in sorted(ICONS.glob("*.svg"))
             if "#FFFFFF" in p.read_text(encoding="utf-8").upper()]
    if not icons:
        raise SystemExit("no master icons found in resources/icons/ffffff/")

    # 5WHY (review 2026-08-17): 旧顺序先删后烘——中断（Ctrl-C/磁盘满）留下
    # 半再生树 + 过期 qrc。改为先烘全量，最后一步才清理过期目录。
    # keep 集合含 DIAG_ACCENT 辅形目录（非调色板 hex，由 bake_diag_accents
    # 生成——如 mysql 的 fbbf24/d97706）。
    accent_dirs = set()
    for pair in DIAG_ACCENT.values():
        for v in pair:
            accent_dirs.add(_resolve_role_ref(v, palettes)[1:].lower())
    keep_dirs = {hx[1:].lower() for hx in hexes} | accent_dirs | {"ffffff"}
    stale_dirs = [sub for sub in sorted(out.iterdir())
                  if sub.is_dir() and re.fullmatch(r"[0-9a-f]{6}", sub.name)
                  and sub.name not in keep_dirs]

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
        # 双套固定色：变体仅属于 Light 块 → FIXED_COLORS_LIGHT，否则 dark 值
        # （AppIcon 运行时按激活主题最近匹配；此判定只决定烘焙哪张固定色表）
        fixed_table = (
            FIXED_COLORS_LIGHT
            if (hx in light_hexes and hx not in dark_hexes)
            else FIXED_COLORS
        )
        if hx == "#FFFFFF":
            sub = master_out
        else:
            sub = out / hx[1:].lower()
            # exist_ok：过期目录清理已移到生成末尾——重复运行时目录仍在
            sub.mkdir(parents=True, exist_ok=True)
            dark = darken_hex(hx, 30)
            used_fixed = False
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
                    # 5WHY (review 2026-08-17): DIAG_ACCENT 表改角色引用后，
                    # 此路径漏了解析——"Light.info" 等非法 SVG 颜色直接烘进
                    # 672 个文件（#000000 哨兵替换是唯一未解析点，18/45 瓦片
                    # 图标强调色失效）。
                    accent = _resolve_role_ref(accent, palettes)
                    colored = colored.replace("#000000", accent)
                # #101010 → second semantic accent (fixed per icon)
                accent2 = SECOND_ACCENT.get(svg.stem, "")
                if accent2:
                    colored = colored.replace("#101010", accent2)
                # #B0000n → n-th fixed color (per icon)
                entries = fixed_table.get(svg.stem)
                if entries is None:
                    # 5WHY (2026-08-17 review): FIXED_COLORS_LIGHT is a
                    # light-theme override subset — icons absent from it must
                    # fall back to the dark FIXED_COLORS entry.  The old
                    # `fixed_table.get(svg.stem, [])` yielded an empty list →
                    # no replacement → the #B0000n sentinel shipped literally
                    # into light SVGs (verified: 4× "#B00001" in
                    # resources/icons/10b981/nd-diag-g3-geoip.svg).
                    entries = FIXED_COLORS.get(svg.stem, [])
                if entries:
                    used_fixed = True
                for i, c in enumerate(entries, 1):
                    colored = colored.replace(f"#B0000{i}", _resolve_role_ref(c, palettes))
                if re.search(r"#B0000[1-9]", colored, re.IGNORECASE):
                    print(f"WARNING: {svg.name} in {sub.name}: unreplaced "
                          f"#B0000n sentinel remains (fixed-color table too short)")
                # #777777 → soft fill（主题感知：随变体主题取 onSurfaceVariant；
                # 5WHY review 2026-08-17: 原硬编码 #64748B 双主题同灰——暗卡上
                # 泥泞、白卡上灰暗）。
                # 5WHY (review round 3 修复)：共享 hex 目录同一文件服务双主题，
                # 之前无差别取 Dark.onSurfaceVariant——亮色主题下这些图标被烘成
                # #94A3B8（白底 ≈2.5:1，原 #64748B ≈4.8:1）细节发灰。共享目录
                # 保留主题无关石板色 #64748B（原行为）。
                if hx in light_hexes and hx not in dark_hexes:
                    soft_fill = palettes["Light"]["onSurfaceVariant"]
                elif hx in dark_hexes and hx not in light_hexes:
                    soft_fill = palettes["Dark"]["onSurfaceVariant"]
                else:
                    soft_fill = "#64748B"
                colored = colored.replace("#777777", soft_fill)
                if re.search(r"(Light|Dark)\.[A-Za-z0-9_]+", colored):
                    raise SystemExit(f"{svg.name} in {sub.name}: unresolved role "
                                     f"reference leaked into output — fix the accent table")
                out_file = sub / svg.name
                out_file.write_text(colored, encoding="utf-8", newline="\n")
        for svg in icons:
            qrc_entries.append(f"icons/{sub.name}/{svg.name}")
        # 5WHY (review 2026-08-17): 共享 hex（两主题都用）只烘一张固定色表——
        # 显式提醒，防止未来图标在亮色主题静默拿到 dark 固定色。
        if used_fixed and hx in dark_hexes and hx in light_hexes:
            print(f"NOTE: {hx[1:].lower()} is shared by both themes — variants "
                  f"baked with the DARK fixed-color table")

    # ── 辅形（masters-45/nd-diag-*-a.svg，#666666 → 逐项辅色对 dark/light）──
    bake_diag_accents(qrc_entries, out, palettes)

    qrc = ['<?xml version="1.0" encoding="utf-8"?>', "<RCC>", '    <qresource prefix="/">']
    qrc += [f"        <file>{e}</file>" for e in qrc_entries]
    qrc += ["    </qresource>", "</RCC>", ""]
    qrc_path.write_text("\n".join(qrc), encoding="utf-8", newline="\n")

    js = [
        "// AUTO-GENERATED by scripts/generate-colored-icons.py — DO NOT EDIT.",
        "// List of pre-generated icon colors for nearest-match selection.",
        ".pragma library",
        "var hexes = [",
    ]
    # 5WHY (review round 4): #FFFFFF 是母版目录而非烘焙变体（内含未替换
    # 哨兵）——最近匹配若选中它，白色/近白图标会渲染损坏内容。从可选
    # 表中排除，白色回落到最近的真实烘焙色。
    js += [f'    "{h}",' for h in hexes if h != "#FFFFFF"]
    js += ["];", ""]
    js_path.write_text("\n".join(js), encoding="utf-8", newline="\n")

    accented = sum(1 for s in icons if ACCENT.get(s.stem, ""))
    print(f"generated {len(hexes)} colors x {len(icons)} icons = "
          f"{len(qrc_entries)} files ({accented} dual-color)")

    # ── 常显主导色（瓦片光晕垫 tint，45 项）──
    write_icon_tints(palettes, tints_path)
    print(f"IconTints.js written: {len(DIAG_ACCENT)} entries")

    # ── 过期目录清理（最后一步；保留 ffffff master 与非色文件）──
    for sub in stale_dirs:
        print(f"removing stale color-variant dir: {sub.name}")
        shutil.rmtree(sub)


def _generated_rel(base: Path) -> set[str]:
    """Relative paths of every file the generator produced under base."""
    return {str(p.relative_to(base)) for p in base.rglob("*") if p.is_file()}


def _real_rel() -> set[str]:
    """Files on disk the generator owns: <hex>/ icon dirs (ffffff excluded —
    it is the master INPUT, never rewritten) plus the three top-level
    outputs.  Non-color files (netanalysis.ico/png, masters-45/) are
    intentionally excluded."""
    rel = set()
    for sub in OUT.iterdir():
        if sub.is_dir() and re.fullmatch(r"[0-9a-f]{6}", sub.name) and sub.name != "ffffff":
            for p in sub.rglob("*"):
                if p.is_file():
                    rel.add(f"resources/icons/{p.relative_to(OUT)}")
    for p in (QRC, JS, TINTS_JS):
        rel.add(str(p.relative_to(ROOT)))
    return rel


def check_drift(out_base: Path) -> int:
    """--check: regenerate into a mirror tree and byte-compare with disk.

    5WHY (2026-08-17 review): the staleness check was a staging-pattern
    heuristic in pre-commit — it never detected content drift, so 22
    light-theme e0f2fe SVGs baked with the wrong fixed-color table shipped
    while every check passed.  Regeneration is deterministic, so the mirror
    diff is exact.
    """
    generate(out_base=out_base)
    generated = _generated_rel(out_base)
    real = _real_rel()
    diffs = []
    for rel in sorted(generated - real):
        diffs.append(f"missing on disk: {rel}")
    for rel in sorted(real - generated):
        diffs.append(f"stale on disk (generator would delete): {rel}")
    for rel in sorted(generated & real):
        if (out_base / rel).read_bytes() != (ROOT / rel).read_bytes():
            diffs.append(f"differs: {rel}")
    if diffs:
        print("icons out of date — run: python scripts/generate-colored-icons.py")
        for d in diffs:
            print("  " + d)
        return 1
    print("icons up to date ✓")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="fail (exit 1) if generated icons/qrc/tints are out of date")
    args = ap.parse_args()
    if args.check:
        with tempfile.TemporaryDirectory() as td:
            return check_drift(Path(td))
    generate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
