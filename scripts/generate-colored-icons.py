#!/usr/bin/env python3
# =============================================================================
# generate-colored-icons.py — 单母版运行时着色元数据管线（v4，方案 B）
# =============================================================================
# 历史：v1-v3 将所有调色板颜色烘焙为静态 SVG 变体（41 个 hex 目录 ≈3400
# 文件），AppIcon 按最近匹配选择。v4（2026-08, 方案 B）取消烘焙：
#
#   Inputs:  src/Common/View/theme/Palette.js    （颜色角色，双主题）
#            resources/icons/ffffff/*.svg        （母版，哨兵色槽）
#   Outputs: resources/icons/master/*.svg        （母版发布，qrc 引用）
#            resources/icon-runtime.json         （逐图标着色元数据）
#            resources/resources_icons.qrc
#            src/Common/View/widgets/IconTints.js（瓦片光晕垫 tint）
#            （IconColors.js 已删除——AppIcon 不再做最近匹配）
#
# 哨兵色槽（母版内字面值，C++ IconProvider 运行时替换，与 C++ 同源）：
#   #FFFFFF → 请求色（主色/渐变起点）    #AAAAAA → 请求色 HSL 加深 30%（渐变深端）
#   #000000 → 语义强调色（accent）       #101010 → 第二强调色（second）
#   #B0000n → 固定多色（fixed，按主题）  #777777 → 柔填充（soft，按主题）
#
# 角色引用（"Dark.primary" 等）在生成期解析为具体 hex——调色板重调自动传播。
#
# Run:  python scripts/generate-colored-icons.py
#       python scripts/generate-colored-icons.py --check   （漂移校验，CI）
# =============================================================================
# 5WHY (review 2026-08-17): future-annotations 使全部注解惰性化——本文件在
# 3.9 及更早的 Python 上原会因 PEP 604（Path | None）在 def 期求值而崩溃
# （macOS 自带 python3 = 3.9）。
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

from palette_common import contrast, luminance, parse_palette, read_palette

ROOT = Path(__file__).resolve().parent.parent
PALETTE = ROOT / "src/Common/View/theme/Palette.js"
ICONS = ROOT / "resources/icons/ffffff"
OUT = ROOT / "resources/icons"
QRC = ROOT / "resources/resources_icons.qrc"
JS = ROOT / "src/Common/View/widgets/IconColors.js"
TINTS_JS = ROOT / "src/Common/View/widgets/IconTints.js"
# v4：运行时着色元数据（AppIcon/IconProvider 消费，替代预烘焙 hex 目录）
RUNTIME_JSON = ROOT / "resources/icon-runtime.json"
MASTER_DIR = ROOT / "resources/icons/master"

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

# ── 逐图标【双主题】语义强调色（#000000 sentinel 的 dark/light 分派）────
# 5WHY (复核 2026-08-21 用户诉求 "WiFi 信息圆圈 i 徽章高亮黄色（dark）"):
# write_icon_runtime 曾把 accent 压成单一值（DIAG_ACCENT 浅色端）——同一
# 颜色用于两主题，dark 下无法给徽章高亮黄（light 下黄色对比度不足）。
# 本表未列出的图标保持旧行为（accent = DIAG_ACCENT 浅色端，两主题同值）。
# wifi-info：dark = #F59E0B（Dark.warning，亮黄，深底实测 8.31:1）；
# light = #C2410C（orange-700，白底实测 5.18:1 过 4.5 文本档——曾选
# Light.warning #EA580C，注释误记 5.1:1，实测仅 3.56:1，贴 3:1 图形底线，
# 1.3px 薄线描不可读）——M3 惯例：同色相跨主题换调（保持"同一徽章"的
# 认知连续性）。
DIAG_ACCENT_THEMED: dict[str, tuple[str, str]] = {
    "nd-diag-g1-wifi-info": ("Dark.warning", "#C2410C"),
    # 5WHY (2026-08-22 用户诉求 "DNS Integrity tick 用亮色系"): 曾单值
    # #06B6D4（Light.tertiary）两主题同色——dark 下不够亮。按主题分派
    # success 亮色系（与 CheckAnimation 动画勾同源同色）：dark #4ADE80
    # （深底高亮绿）、light #10B981（亮绿 vivid，浅底仍饱和）。
    "nd-diag-g3-dns-integrity": ("Dark.success", "Light.success"),
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
    # 5WHY (2026-08-19 浅色可读): >_ 提示符+协议文字（#B00005）曾用 #FFFFFF
    # 请求色——light 下 iconInk #0C4A6E 压在深屏 #0F172A 上仅 1.89:1，终端
    # 文字不可见。改固定槽，颜色经 Palette.js terminalInk 角色双主题配对。
    "nd-diag-g5-ssh": ["#1E293B", "#F87171", "#FBBF24", "#4ADE80", "Dark.terminalInk"],
    "nd-diag-g5-ftp": ["#1E293B", "#F87171", "#FBBF24", "#4ADE80", "Dark.terminalInk"],
    "nd-diag-g5-telnet": ["#1E293B", "#F87171", "#FBBF24", "#4ADE80", "Dark.terminalInk"],
    # IP 配置：屏幕 #777777 柔填充 + Hershey 数字（#B00001）。数字曾用
    # #FFFFFF 请求色——light 下 iconInk #0C4A6E 压在石板屏 #475569 上仅
    # 1.25:1（用户诉求 "1.1.1.1 看不见"）。dark 深墨 #0F172A / light 白。
    "nd-diag-g1-ip-config": ["#0F172A"],
    # DHCP：地球线框（#B00002）曾用 #FFFFFF 请求色——light 下深蓝线框压在
    # 石板字母 #475569 上仅 1.25:1。改固定槽双主题配色；首槽 #1E293B 是
    # write_icon_tints 首槽亮度门占位（lum<0.15 → tint 回退 DIAG_ACCENT
    # 琥珀，光晕垫色保持不变）。
    "nd-diag-g1-dhcp": ["#1E293B", "Dark.terminalInk"],
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
    # HTTP 图标：背景 = 1:1 复刻 blankfile.png（页面 #007CC9 + 折角 #0094F5），内部图形保留主题色
    # 5WHY (2026-08-19 浅色可读): HTML 文字/白图形（#B0000n）曾用 #FFFFFF
    # 请求色——页面蓝上 2.3-3.0:1（dark 2.35/light 2.95）。与 DB 四联同
    # 模式：dark 白、light 深墨 #0F172A（5.6-4.4:1）。
    # 5WHY (2026-08-23 S1): security/http-headers/curl 三图删描摹底形出族
    # （同源底形互斥失败）——固定槽随之移除，tint 回退 DIAG_ACCENT。
    "nd-diag-g5-http-timing": ["#007CC9", "#0094F5", "#FBBF24", "#FFFFFF"],
    "nd-diag-g5-http-redirect": ["#007CC9", "#0094F5", "#FFFFFF"],
    "nd-diag-g5-ssl-certificate": ["#007CC9", "#0094F5", "#FBBF24", "#4ADE80", "#FFFFFF"],
    # 5WHY (2026-08-23 用户复决): security/http-headers/curl 三图恢复
    # blankfile 底形样式（S1 瘦身方案全部回退）——用户裁定底形家族识别度
    # 优先于剪影互斥；同源碰撞登记为已知接受项。
    "nd-diag-g5-security-headers": ["#007CC9", "#0094F5", "#FFFFFF"],
    "nd-diag-g5-http-headers": ["#007CC9", "#0094F5", "#FFFFFF"],
    "nd-diag-g5-curl-verbose": ["#007CC9", "#0094F5", "#FFFFFF"],
    "nd-diag-g5-http-compression": ["#007CC9", "#0094F5", "#FFFFFF"],
}

# ── 浅色主题固定色（#B0000n → 第 n 个固定色，仅 light 变体使用）────────────────
# 双套规则：固定色按主题分 dark/light，烘焙时按变体所属主题选择；
# 未列出的图标浅色主题沿用 FIXED_COLORS（dark 值）。
FIXED_COLORS_LIGHT: dict[str, list[str]] = {
    # A 类：页面双套（dark #007CC9 / light #0094F5，折角 #37AAF9）
    "nd-diag-g2-network-profile": ["#0094F5", "#37AAF9"],
    "nd-diag-g5-mqtt": ["#0094F5", "#37AAF9", "#22C55E"],
    "nd-diag-g5-http-timing": ["#0094F5", "#37AAF9", "#F59E0B", "#0F172A"],
    "nd-diag-g5-http-redirect": ["#0094F5", "#37AAF9", "#0F172A"],
    "nd-diag-g5-ssl-certificate": ["#0094F5", "#37AAF9", "#F59E0B", "#10B981", "#0F172A"],
    "nd-diag-g5-security-headers": ["#0094F5", "#37AAF9", "#0F172A"],
    "nd-diag-g5-http-headers": ["#0094F5", "#37AAF9", "#0F172A"],
    "nd-diag-g5-curl-verbose": ["#0094F5", "#37AAF9", "#0F172A"],
    "nd-diag-g5-http-compression": ["#0094F5", "#37AAF9", "#0F172A"],
    # DB 四联 light：页面浅蓝 + 深色细节（白描边/白字 → 深 #0F172A，圆柱加深）
    "nd-diag-g5-mysql": ["#0094F5", "#37AAF9", "#0F172A", "#014A75", "#0369A1", "#0284C7"],
    "nd-diag-g5-postgres": ["#0094F5", "#37AAF9", "#0F172A", "#014A75", "#0369A1", "#0284C7"],
    "nd-diag-g5-redis": ["#0094F5", "#37AAF9", "#0F172A", "#014A75", "#0369A1", "#0284C7"],
    "nd-diag-g5-mongodb": ["#0094F5", "#37AAF9", "#0F172A", "#014A75", "#0369A1", "#0284C7"],
    # B 类：终端三协议 light 屏幕保持深黑；proxy/ipv6/cellular/internet light 加深
    "nd-diag-g5-ssh": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80", "Light.terminalInk"],
    "nd-diag-g5-ftp": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80", "Light.terminalInk"],
    "nd-diag-g5-telnet": ["#0F172A", "#F87171", "#FBBF24", "#4ADE80", "Light.terminalInk"],
    # IP 配置 light：深屏上白色数字（#B00001）；DHCP light：深墨线框 #0F172A
    # （#B00002）——白卡上 9.5:1 保持球框可见，字母交叉处 1.25→2.4:1
    "nd-diag-g1-ip-config": ["#FFFFFF"],
    "nd-diag-g1-dhcp": ["#1E293B", "#0F172A"],
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


def write_icon_runtime(palettes: dict, json_path: Path = RUNTIME_JSON) -> None:
    """v4：输出运行时着色元数据（单母版 + C++ IconProvider 消费）。

    每图标记录 #000000→accent、#101010→second、#777777→softFill（按主题）、
    #B0000n→fixed（按主题）。角色引用在生成期解析（与烘焙同源），
    C++ 端不再复制任何调色板表。
    """
    dark_soft = palettes["Dark"].get("onSurfaceVariant", "#64748B")
    light_soft = palettes["Light"].get("onSurfaceVariant", "#64748B")
    data: dict = {"version": 1, "icons": {}}
    for svg in sorted(ICONS.glob("*.svg")):
        stem = svg.stem
        accent = ACCENT.get(stem, "")
        if accent:
            accent = _resolve_role_ref(accent, palettes)
        second = SECOND_ACCENT.get(stem, "")
        if second:
            second = _resolve_role_ref(second, palettes)
        fixed_dark = [_resolve_role_ref(c, palettes) for c in FIXED_COLORS.get(stem, [])]
        fl = FIXED_COLORS_LIGHT.get(stem)
        if fl is None:
            fl = FIXED_COLORS.get(stem, [])   # 浅色未列出 → 沿用 dark 表（烘焙同规则）
        fixed_light = [_resolve_role_ref(c, palettes) for c in fl]
        themed = DIAG_ACCENT_THEMED.get(stem)
        # 5WHY (复核 2026-08-21): 双主题强调色分派（DIAG_ACCENT_THEMED）。
        # 仅列入表的图标发射双主题键（现仅 wifi-info）；其余不发射——
        # C++ 缺键读空回退 accent（单一回退链，JSON 不再为 166 个未列入
        # 图标重复 accent 值）。
        entry = {
            "accent": accent,
            "second": second,
            "softDark": dark_soft,
            "softLight": light_soft,
            "fixedDark": fixed_dark,
            "fixedLight": fixed_light,
        }
        if themed:
            entry["accentDark"] = _resolve_role_ref(themed[0], palettes)
            entry["accentLight"] = _resolve_role_ref(themed[1], palettes)
        data["icons"][stem] = entry
    json_path.write_text(json.dumps(data, indent=1) + "\n",
                         encoding="utf-8", newline="\n")


def _resolve_outputs(out_base: Path | None):
    """Output paths — redirected under out_base in --check mode."""
    if out_base is None:
        return OUT, QRC, JS, TINTS_JS, RUNTIME_JSON
    return (
        out_base / "resources/icons",
        out_base / "resources/resources_icons.qrc",
        out_base / "src/Common/View/widgets/IconColors.js",
        out_base / "src/Common/View/widgets/IconTints.js",
        out_base / "resources/icon-runtime.json",
    )


def generate(out_base: Path | None = None) -> None:
    """Full regeneration (master publish + qrc + icon-runtime.json + tints).

    out_base=None writes the real outputs; a redirect (--check) writes into
    a mirror tree for drift comparison.
    """
    out, qrc_path, js_path, tints_path, runtime_json = _resolve_outputs(out_base)
    # Ensure output roots exist (--check mode starts from an empty mirror).
    out.mkdir(parents=True, exist_ok=True)
    qrc_path.parent.mkdir(parents=True, exist_ok=True)
    js_path.parent.mkdir(parents=True, exist_ok=True)
    tints_path.parent.mkdir(parents=True, exist_ok=True)
    runtime_json.parent.mkdir(parents=True, exist_ok=True)
    # Single Palette.js read; every parser works on the same text
    # (5WHY review 2026-08-17: the old code re-read and re-parsed the file
    # three times per run with three divergent regex parsers).
    text = read_palette()
    palettes = parse_palette(text)

    # v4：单母版管线。fff 母版发布为 qrc:/icons/master/<name>.svg，
    # 全部着色参数（accent/second/soft/fixed 按主题）进入 icon-runtime.json，
    # 运行时由 C++ IconProvider 精确着色 —— 不再烘焙 41 个 hex 目录。
    icons = [p for p in sorted(ICONS.glob("*.svg"))
             if "#FFFFFF" in p.read_text(encoding="utf-8").upper()]
    if not icons:
        raise SystemExit("no master icons found in resources/icons/ffffff/")

    # v4：keep 仅母版输入 ffffff 与母版发布目录 master —— "master" 六字符
    # 全落在 [0-9a-f]，否则会被 hex 正则误判为过期变体目录而删除。
    keep_dirs = {"ffffff", "master"}
    stale_dirs = [sub for sub in sorted(out.iterdir())
                  if sub.is_dir() and re.fullmatch(r"[0-9a-f]{6}", sub.name)
                  and sub.name not in keep_dirs]

    qrc_entries = []
    # ── v4：发布母版目录 resources/icons/master/（qrc 引用物理路径）──
    # 镜像模式（--check）：out 已是 <base>/resources/icons，故 master 位于
    # out/master —— 与 _real_rel()/check_drift 的相对路径保持一致。
    master_out_dir = MASTER_DIR if out_base is None else out / "master"
    if master_out_dir.is_dir():
        shutil.rmtree(master_out_dir)
    master_out_dir.mkdir(parents=True, exist_ok=True)
    for svg in icons:
        shutil.copyfile(svg, master_out_dir / svg.name)
        qrc_entries.append(f"icons/master/{svg.name}")
    # ── v4：运行时着色元数据 ──
    write_icon_runtime(palettes, runtime_json)
    qrc_entries.append("icon-runtime.json")

    qrc = ['<?xml version="1.0" encoding="utf-8"?>', "<RCC>", '    <qresource prefix="/">']
    qrc += [f"        <file>{e}</file>" for e in qrc_entries]
    qrc += ["    </qresource>", "</RCC>", ""]
    qrc_path.write_text("\n".join(qrc), encoding="utf-8", newline="\n")

    # v4：IconColors.js 已删除（AppIcon 精确着色不再需要最近匹配索引）
    if js_path.exists():
        js_path.unlink()

    print(f"generated {len(icons)} master icons published to qrc:/icons/master + icon-runtime.json")

    # ── 常显主导色（瓦片光晕垫 tint，45 项）──
    write_icon_tints(palettes, tints_path)
    print(f"IconTints.js written: {len(DIAG_ACCENT)} entries")

    # ── 过期目录清理（最后一步；保留 ffffff master 与非色文件）──
    for sub in stale_dirs:
        print(f"removing stale color-variant dir: {sub.name}")
        shutil.rmtree(sub)


def _generated_rel(base: Path) -> set[str]:
    """Relative paths (posix separators) of every file the generator
    produced under base — Windows Path str uses backslashes, which must be
    normalized so check_drift's set comparison is separator-agnostic."""
    return {p.relative_to(base).as_posix() for p in base.rglob("*") if p.is_file()}


def _real_rel() -> set[str]:
    """Files on disk the generator owns in v4:
    resources/icons/master/ (published masters) + the top-level outputs
    (qrc / icon-runtime.json / IconTints.js).  Non-color files
    (netanalysis.ico/png, masters-45/, resources/icons/ffffff input) are
    intentionally excluded."""
    rel = set()
    if MASTER_DIR.is_dir():
        for p in MASTER_DIR.rglob("*"):
            if p.is_file():
                # as_posix()：Windows 下 Path str 为反斜杠，与 _generated_rel
                # 的纯正斜杠镜像相对路径不一致会导致 --check 误报缺失。
                rel.add("/".join(("resources", "icons",
                                  p.relative_to(MASTER_DIR.parent).as_posix())))
    for p in (QRC, RUNTIME_JSON, TINTS_JS):
        rel.add(p.relative_to(ROOT).as_posix())
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
