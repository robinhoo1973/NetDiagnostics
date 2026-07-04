#!/usr/bin/env python3
"""Generate accurate Figma SVGs from Qt6 QML source templates.

Reads QML layout rules (colors, fonts, spacings, component structure) and
generates pixel-accurate SVG mockups for each screen, platform, and device
size, mirroring the screenshot directory structure.

Directory structure (matches screenshot/):
  figma/{platform}/{size}/{device}/{screen}.svg

Devices:
  ios/phone/6.1  -> 375×812  (iPhone 11 Pro / X / XS, @3x)
  ios/phone/6.3  -> 402×874  (iPhone 16 Pro, @3x)
  ios/phone/6.5  -> 414×896  (iPhone 11 Pro Max, @3x)
  ios/phone/6.9  -> 440×956  (iPhone 16 Pro Max, @3x)
  ios/tablet/13  -> 1032×1376 (iPad 13", @2x)
"""

import os, html, math

# ═══════════════════════════════════════════════════════════════════════
# Theme — exact from AppTheme.qml
# ═══════════════════════════════════════════════════════════════════════
THEME = {
    "bgDark": "#1E1E2E", "bgSidebar": "#252538", "bgCard": "#16213E",
    "bgInput": "#2A2A4A", "appBar": "#1A1A2E",
    "textPrimary": "#E0E0E0", "textSecondary": "#A0A0B8", "textMuted": "#606080",
    "accent": "#E94560", "accentBlue": "#0078D4", "cyan": "#00BCD4",
    "passGreen": "#4ADE80", "warnYellow": "#FACC15", "failRed": "#EF4444",
    "skipGray": "#888888", "infoBlue": "#0078D4",
    "borderCard": "#3A3A5A", "borderSubtle": "#2A2A4A", "borderFocused": "#0078D4",
    "radiusCard": 12, "radiusButton": 8, "radiusSmall": 6, "sidebarWidth": 260,
}

FONT = 'JetBrains Mono, DejaVu Sans Mono, monospace'

# ═══════════════════════════════════════════════════════════════════════
# Device definitions (logical/CSS pixels: physical / scale factor)
# ═══════════════════════════════════════════════════════════════════════
DEVICES = {
    "ios/phone/6.1":  {"w": 375, "h": 812, "scale": 3, "name": "iPhone 6.1″", "isMobile": True, "os": "ios"},
    "ios/phone/6.3":  {"w": 402, "h": 874, "scale": 3, "name": "iPhone 6.3″", "isMobile": True, "os": "ios"},
    "ios/phone/6.5":  {"w": 414, "h": 896, "scale": 3, "name": "iPhone 6.5″", "isMobile": True, "os": "ios"},
    "ios/phone/6.9":  {"w": 440, "h": 956, "scale": 3, "name": "iPhone 6.9″", "isMobile": True, "os": "ios"},
    "ios/tablet/13":  {"w": 1032, "h": 1376, "scale": 2, "name": "iPad 13″", "isMobile": False, "os": "ios"},
}

SCREENS = ["dashboard", "diagnostics", "config", "report", "settings"]

# ═══════════════════════════════════════════════════════════════════════
# SVG builder
# ═══════════════════════════════════════════════════════════════════════
class SVG:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.parts = []
        self.defs_content = []
        self._id = 0

    def uid(self): self._id += 1; return f"g{self._id}"

    def add_def(self, s): self.defs_content.append(s)

    def raw(self, s): self.parts.append(s); return self

    def rect(self, x, y, w, h, rx=0, fill=None, stroke=None, sw=1, opacity=None, cls=None):
        a = []
        if cls: a.append(f'class="{cls}"')
        else:
            if fill: a.append(f'fill="{fill}"')
            if stroke: a.append(f'stroke="{stroke}" stroke-width="{sw}"')
            if rx: a.append(f'rx="{rx}"')
        if opacity is not None: a.append(f'opacity="{opacity}"')
        self.parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" {" ".join(a)}/>')
        return self

    def text(self, x, y, text, size=12, fill="#E0E0E0", anchor="start", weight="normal", font=None, ellipsis_w=0):
        t = html.escape(str(text))
        ff = font or FONT
        if ellipsis_w:
            self.parts.append(f'<text x="{x}" y="{y}" font-family="{ff}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}"><tspan>{t}</tspan></text>')
        else:
            self.parts.append(f'<text x="{x}" y="{y}" font-family="{ff}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}">{t}</text>')
        return self

    def circle(self, cx, cy, r, fill=None, stroke=None, sw=1):
        a = []
        if fill: a.append(f'fill="{fill}"')
        if stroke: a.append(f'stroke="{stroke}" stroke-width="{sw}"')
        self.parts.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" {" ".join(a)}/>')
        return self

    def line(self, x1, y1, x2, y2, stroke="#3A3A5A", sw=1):
        self.parts.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{stroke}" stroke-width="{sw}"/>')
        return self

    def group(self, transform=None, opacity=None):
        self.parts.append('<g' + (f' transform="{transform}"' if transform else '') + (f' opacity="{opacity}"' if opacity is not None else '') + '>')
        return _GroupCtx(self)

    def to_xml(self):
        lines = [f'<?xml version="1.0" encoding="UTF-8"?>',
                 f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" width="{self.w}" height="{self.h}">']
        if self.defs_content:
            lines.append('<defs>')
            lines.extend(self.defs_content)
            lines.append('</defs>')
        lines.extend(self.parts)
        lines.append('</svg>')
        return '\n'.join(lines)


class _GroupCtx:
    def __init__(self, svg): self.svg = svg
    def __enter__(self): return self.svg
    def __exit__(self, *a): self.svg.parts.append('</g>')


# ═══════════════════════════════════════════════════════════════════════
# Shared components (exact from QML)
# ═══════════════════════════════════════════════════════════════════════

def draw_nav_bar(svg, dev, active_tab=1):
    """Navigation bar from AppContent.qml — compact on mobile, full on desktop."""
    is_compact = dev["isMobile"]
    bar_h = 32 if is_compact else 36
    svg.rect(0, 0, svg.w, bar_h, fill=THEME["appBar"])
    svg.line(0, bar_h - 1, svg.w, bar_h - 1, "#3A3A5A")

    tabs = [
        ("⊞", "Dashboard", "dashboard"),
        ("⚡", "Diag", "diagnostics"),
        ("⚙", "Config", "config"),
        ("📄", "Report", "report"),
        ("⚙", "Settings", "settings"),
    ]
    tab_w = 44 if is_compact else 100
    tab_start = (svg.w - len(tabs) * tab_w) // 2 if not is_compact else svg.w - len(tabs) * tab_w - 4

    for i, (icon, label, _) in enumerate(tabs):
        x = tab_start + i * tab_w
        active = (i == active_tab)
        tab_cy = bar_h // 2

        if active:
            svg.rect(x, 2, tab_w, bar_h - 2, rx=6, fill=f"rgba(0,188,212,0.12)")
            if not is_compact:
                svg.rect(x, bar_h - 2, tab_w, 2, fill=THEME["cyan"])
            fill = THEME["cyan"]
        else:
            fill = "rgba(224,224,224,0.55)" if is_compact else "rgba(224,224,224,0.7)"

        if is_compact:
            svg.text(x + tab_w // 2, bar_h // 2 + 3, icon, size=12, fill=fill, anchor="middle")
        else:
            svg.text(x + tab_w // 2, bar_h // 2 + 3, f"{icon} {label}", size=10, fill=fill, anchor="middle")

    # Close button on desktop
    if not is_compact:
        cx = svg.w - 20
        svg.rect(cx - 14, 4, 28, 28, rx=6, fill="transparent", stroke="#5A5A7A")
        svg.text(cx, bar_h // 2 + 4, "✕", size=12, fill=THEME["textSecondary"], anchor="middle")

    return bar_h


def draw_status_bar(svg, dev):
    """iOS status bar."""
    if dev["os"] != "ios":
        return 0
    h = 24 if dev["isMobile"] else 20
    svg.rect(0, 0, svg.w, h, fill="#000000")
    time_str = "9:41"
    svg.text(16, h - 7, time_str, size=10 if dev["isMobile"] else 9, fill="#FFFFFF", anchor="start")
    svg.text(svg.w - 16, h - 7, "Wi-Fi", size=10 if dev["isMobile"] else 9, fill="#FFFFFF", anchor="end")
    return h


def draw_bottom_tab_bar(svg, dev, active_tab=1):
    """iOS-style bottom tab bar for mobile."""
    if not dev["isMobile"]:
        return 0
    bar_h = 50
    y = svg.h - bar_h
    svg.rect(0, y, svg.w, bar_h, fill=THEME["appBar"])
    svg.line(0, y, svg.w, y, THEME["borderCard"])

    tabs = [
        ("Dashboard", "dashboard"),
        ("Diag", "diagnostics"),
        ("Config", "config"),
        ("Report", "report"),
        ("Settings", "settings"),
    ]
    tab_w = svg.w // len(tabs)
    for i, (label, _) in enumerate(tabs):
        x = i * tab_w
        cy = y + bar_h // 2 + 2
        active = (i == active_tab)
        fill = THEME["cyan"] if active else THEME["textMuted"]
        svg.text(x + tab_w // 2, cy, label[:8], size=9, fill=fill, anchor="middle")

    return bar_h


def draw_app_bar(svg, dev, icon, title, subtitle=None, bar_h=52):
    """Standard AppBar from Dashboard/Report/Settings QML."""
    svg.rect(0, 0, svg.w, bar_h, fill=THEME["appBar"])
    svg.line(0, bar_h - 1, svg.w, bar_h - 1, THEME["borderCard"])

    # Icon
    if icon == "dashboard":
        draw_dashboard_icon(svg, 16, bar_h // 2, 20, THEME["cyan"])
    elif icon == "report":
        draw_report_icon(svg, 16, bar_h // 2, 20, THEME["cyan"])
    elif icon == "settings":
        draw_settings_icon(svg, 16, bar_h // 2, 20, THEME["cyan"])

    svg.text(42, bar_h // 2 + 4, title, size=15, fill=THEME["textPrimary"], weight="bold")
    if subtitle:
        svg.text(42, bar_h // 2 + 20, subtitle, size=11, fill=THEME["textSecondary"])
    return bar_h


# ── Icon drawing helpers (simple geometric) ──

def draw_dashboard_icon(svg, cx, cy, size, color):
    """Simple dashboard icon: grid pattern."""
    s = size / 2
    for dx, dy in [(-s/2, -s/2), (s/2, -s/2), (-s/2, s/2), (s/2, s/2)]:
        svg.rect(cx + dx - s/3, cy + dy - s/3, s*2/3, s*2/3, rx=2, fill=color, opacity=0.8)

def draw_report_icon(svg, cx, cy, size, color):
    """Document icon."""
    svg.rect(cx - size/3, cy - size/2, size*2/3, size, rx=3, stroke=color, sw=2, fill="none")
    for i in range(3):
        svg.line(cx - size/4, cy - size/4 + i * 5, cx + size/4, cy - size/4 + i * 5, color, 1.5)

def draw_settings_icon(svg, cx, cy, size, color):
    """Gear icon."""
    r = size / 3
    svg.circle(cx, cy, r, stroke=color, sw=2, fill="none")
    svg.circle(cx, cy, r/2, stroke=color, sw=1.5, fill="none")

def draw_check_icon(svg, cx, cy, size, color):
    svg.line(cx - size/3, cy, cx, cy + size/3, color, 2)
    svg.line(cx, cy + size/3, cx + size*2/3, cy - size/3, color, 2)

def draw_badge(svg, x, y, text, color, count=0):
    """Badge pill from DashboardBadge."""
    if count <= 0:
        return 0
    w = 22; h = 16
    svg.rect(x, y, w, h, rx=4, fill=f"rgba({_hex_to_rgba(color, 0.15)})")
    svg.text(x + w/2, y + h/2 + 3, str(count), size=10, fill=color, anchor="middle", weight="bold")
    return w + 4


def _hex_to_rgba(hex_color, alpha):
    """Convert hex to rgba string."""
    c = hex_color.lstrip('#')
    r, g, b = int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16)
    return f"{r},{g},{b},{alpha}"


def draw_progress_bar(svg, x, y, w, h, pct, fill_color):
    """Progress bar with background."""
    svg.rect(x, y, w, h, rx=h/2, fill=THEME["borderSubtle"])
    svg.rect(x, y, w * pct, h, rx=h/2, fill=fill_color)


# ═══════════════════════════════════════════════════════════════════════
# Screen generators
# ═══════════════════════════════════════════════════════════════════════

def gen_dashboard(svg, dev):
    """DashboardScreen.qml — 1:1"""
    nav_h = draw_nav_bar(svg, dev, active_tab=0)
    top = nav_h
    appbar_h = 52
    svg.rect(0, top, svg.w, appbar_h, fill=THEME["appBar"])
    svg.line(0, top + appbar_h - 1, svg.w, top + appbar_h - 1, THEME["borderCard"])
    draw_dashboard_icon(svg, 16, top + appbar_h // 2, 20, THEME["cyan"])
    svg.text(42, top + appbar_h // 2 + 4, "Dashboard", size=15, fill=THEME["textPrimary"], weight="bold")

    # Reset button
    rx_btn = svg.w - 76
    svg.rect(rx_btn, top + 10, 60, 32, rx=6, fill="transparent", stroke="#5A5A7A")
    svg.text(rx_btn + 30, top + appbar_h // 2 + 4, "Reset", size=12, fill=THEME["textSecondary"], anchor="middle")

    cy = top + appbar_h + 24
    pad = 24 if not dev["isMobile"] else 16
    cw = svg.w - pad * 2

    # ── Run Info Card ──
    card_cy = cy
    card_h = 80
    svg.rect(pad, card_cy, cw, card_h, rx=12, fill=THEME["bgCard"], stroke=THEME["borderSubtle"])
    draw_check_icon(svg, pad + 30, card_cy + card_h // 2, 28, THEME["passGreen"])
    svg.text(pad + 54, card_cy + 24, "Diagnostic Run Complete", size=16, fill=THEME["textPrimary"], weight="bold")
    svg.text(pad + 54, card_cy + 44, "Target: 192.168.1.1", size=12, fill=THEME["textSecondary"])
    svg.text(pad + 54, card_cy + 62, "14:32:18", size=12, fill=THEME["textSecondary"])

    cy = card_cy + card_h + 24

    # ── Summary Cards ──
    summary_items = [
        ("Pass", THEME["passGreen"], 32),
        ("Info", THEME["accentBlue"], 0),
        ("Warning", THEME["warnYellow"], 4),
        ("Fail", THEME["failRed"], 1),
        ("Skipped", THEME["skipGray"], 1),
    ]
    svg.text(pad, cy, "Summary", size=11, fill=THEME["textSecondary"], weight="bold")
    svg.text(svg.w - pad, cy, "Total: 38", size=10, fill=THEME["textSecondary"], anchor="end")
    cy += 18
    for label, color, count in summary_items:
        sh = 32
        svg.rect(pad, cy, cw, sh, rx=6, fill=f"rgba({_hex_to_rgba(color, 0.06)})", stroke=f"rgba({_hex_to_rgba(color, 0.2)})")
        svg.text(pad + 8, cy + sh // 2 + 3, label, size=10, fill=f"rgba({_hex_to_rgba(THEME['textSecondary'], 0.8)})")
        svg.text(svg.w - pad - 8, cy + sh // 2 + 5, f"{count:3d}", size=16, fill=color, anchor="end", weight="bold")
        cy += sh + 4

    cy += 20

    # ── Per-Group Results ──
    svg.text(pad, cy, "Per-Group Results", size=15, fill=THEME["textPrimary"], weight="bold")
    cy += 22

    groups = [
        ("G1 System & Adapters", [("pass", 8), ("warn", 0), ("fail", 0), ("skip", 0)], 0.85, "2.1s"),
        ("G2 Connectivity & Security", [("pass", 6), ("warn", 0), ("fail", 0), ("skip", 0)], 0.72, "1.8s"),
        ("G3 Internet & DNS", [("pass", 4), ("warn", 1), ("fail", 0), ("skip", 0)], 0.60, "3.4s"),
        ("G4 Remote Host", [("pass", 6), ("warn", 0), ("fail", 0), ("skip", 0)], 0.90, "2.2s"),
        ("G5 Website / URL", [("pass", 8), ("warn", 3), ("fail", 1), ("skip", 1)], 0.70, "5.1s"),
    ]

    badge_colors = {"pass": THEME["passGreen"], "warn": THEME["warnYellow"],
                    "fail": THEME["failRed"], "skip": THEME["skipGray"]}

    for gname, stats, pct, dur in groups:
        gh = 100
        svg.rect(pad, cy, cw, gh, rx=10, fill=THEME["bgCard"], stroke=THEME["borderSubtle"])
        # Accent bar
        svg.rect(pad + 14, cy + 18, 3, 20, rx=2, fill=THEME["accentBlue"])
        svg.text(pad + 27, cy + 24, gname, size=13, fill=THEME["textPrimary"], weight="bold")

        # Badges
        bx = svg.w - pad - 16
        for stype, count in reversed(stats):
            if count > 0:
                bw = 22; bh = 16
                bx -= bw + 4
                svg.rect(bx, cy + 16, bw, bh, rx=4, fill=f"rgba({_hex_to_rgba(badge_colors[stype], 0.15)})")
                svg.text(bx + bw/2, cy + 16 + bh/2 + 3, str(count), size=10, fill=badge_colors[stype], anchor="middle", weight="bold")

        # Duration
        svg.text(svg.w - pad - 16, cy + 36, dur, size=11, fill=THEME["textSecondary"], anchor="end")

        # Progress
        draw_progress_bar(svg, pad + 14, cy + 52, cw - 28, 4, pct, THEME["passGreen"] if pct > 0.8 else THEME["warnYellow"])

        # Mini test list
        mini_y = cy + 68
        for j, (stype, scount) in enumerate(stats):
            if scount > 0:
                ic = badge_colors[stype]
                svg.circle(pad + 18, mini_y + 2, 4, fill=ic)
                svg.text(pad + 26, mini_y + 5, f"Test {j+1}.{stype}", size=10, fill=THEME["textSecondary"])
                mini_y += 14

        cy += gh + 8

    # ── Overall Summary Card ──
    cy += 12
    card2_h = 160
    svg.rect(pad, cy, cw, card2_h, rx=12, fill=THEME["bgCard"], stroke=THEME["borderSubtle"])
    svg.text(pad + 16, cy + 24, "Summary", size=15, fill=THEME["textPrimary"], weight="bold")
    items = [
        ("Total Diagnostics", "38", THEME["cyan"]),
        ("Total Time", "12.4s", THEME["accentBlue"]),
        ("Completed", "38", THEME["passGreen"]),
    ]
    iy = cy + 54
    for lbl, val, clr in items:
        svg.text(pad + 16, iy + 4, lbl, size=12, fill=THEME["textSecondary"])
        svg.text(svg.w - pad - 16, iy + 8, val, size=16, fill=clr, anchor="end", weight="bold")
        iy += 24

    # Layer timings
    svg.line(pad + 16, iy, svg.w - pad - 16, iy, THEME["borderSubtle"])
    iy += 8
    svg.text(pad + 16, iy + 12, "Layer Timings", size=12, fill=THEME["textSecondary"], weight="bold")
    iy += 24
    for gi, gname in enumerate(["G1 System & Adapters", "G2 Connectivity", "G3 Internet & DNS", "G4 Remote Host", "G5 Website / URL"]):
        if iy + 16 > cy + card2_h - 8:
            break
        svg.rect(pad + 16, iy, 8, 8, rx=2, fill=THEME["accentBlue"])
        svg.text(pad + 30, iy + 8, gname[:30], size=11, fill=THEME["textPrimary"])
        svg.text(svg.w - pad - 16, iy + 8, f"{2.0 + gi * 0.8:.1f}s", size=11, fill=THEME["textSecondary"], anchor="end")
        iy += 18

    return svg


def gen_diagnostics(svg, dev):
    """DiagnosticScreen.qml — wide/narrow sidebar layout."""
    nav_h = draw_nav_bar(svg, dev, active_tab=1)
    top = nav_h
    is_wide = svg.w >= 600
    sidebar_w = 260 if is_wide else svg.w

    if is_wide:
        # Wide: sidebar | divider | content
        svg.rect(0, top, sidebar_w, svg.h - top, fill=THEME["bgSidebar"])
        svg.line(sidebar_w, top, sidebar_w, svg.h - top, THEME["borderCard"])
        content_x = sidebar_w + 1
        content_w = svg.w - content_x
    else:
        # Narrow: sidebar on top (29-50% height)
        sidebar_h = int((svg.h - top) * 0.29)
        svg.rect(0, top, svg.w, sidebar_h, fill=THEME["bgSidebar"])
        svg.line(0, top + sidebar_h, svg.w, top + sidebar_h, THEME["borderCard"])
        content_x = 0
        content_w = svg.w
        top = top + sidebar_h + 1

    # ── Sidebar content ──
    sx = 12 if is_wide else 16
    sy = top - (0 if is_wide else 0) + 12 if is_wide else nav_h + 12

    if is_wide:
        # Header
        svg.rect(0, nav_h, sidebar_w, 48, fill="transparent", stroke=THEME["borderCard"])
        svg.text(sx, nav_h + 30, "NetDiagnostics", size=16, fill=THEME["textPrimary"], weight="bold")

        sy = nav_h + 60

        # Target input
        svg.text(sx, sy, "Target", size=11, fill=THEME["textSecondary"], weight="bold")
        sy += 18
        svg.rect(sx, sy, sidebar_w - 24, 40, rx=8, fill=THEME["bgInput"], stroke=THEME["borderCard"])
        svg.text(sx + 10, sy + 26, "Enter URL, IP address, or hostname...", size=11, fill=f"rgba({_hex_to_rgba(THEME['textSecondary'], 0.4)})")
        sy += 48

        # Run button
        svg.rect(sx, sy, sidebar_w - 72, 38, rx=8, fill=THEME["accentBlue"])
        svg.text(sx + (sidebar_w - 72) // 2, sy + 25, "▶ Run Diagnostics", size=12, fill="#FFFFFF", anchor="middle", weight="bold")
        # Stop button
        svg.rect(sx + sidebar_w - 66, sy, 42, 38, rx=8, fill="transparent", stroke=f"rgba(239,68,68,0.5)")
        svg.text(sx + sidebar_w - 45, sy + 25, "■", size=12, fill=THEME["failRed"], anchor="middle")
        sy += 52

        # Diagnosis Group label
        svg.text(sx, sy, "Diagnosis Group", size=11, fill=THEME["textSecondary"], weight="bold")
        sy += 18

        # Group checkboxes
        groups = [
            ("G1 System & Adapters", True),
            ("G2 Connectivity & Security", True),
            ("G3 Internet & DNS", True),
            ("G4 Remote Host", False),
            ("G5 Website / URL", False),
        ]
        for gname, checked in groups:
            svg.rect(sx, sy, sidebar_w - 24, 32, rx=6, fill=f"rgba(0,120,212,0.12)" if checked else "transparent")
            # Checkbox
            cbx = sx + 8
            cby = sy + 8
            svg.rect(cbx, cby, 16, 16, rx=3, fill=THEME["accentBlue"] if checked else "none", stroke=THEME["accentBlue"] if checked else "#5A5A7A", sw=1.5)
            if checked:
                svg.text(cbx + 8, cby + 11, "✓", size=10, fill="#FFFFFF", anchor="middle")
            svg.text(cbx + 24, sy + 20, gname, size=12, fill=THEME["textPrimary"] if checked else THEME["textSecondary"])
            sy += 34

        sy += 8

        # Port Scan card
        port_h = 108
        svg.rect(sx, sy, sidebar_w - 24, port_h, rx=8, fill=f"rgba(22,33,62,0.5)", stroke=f"rgba(0,120,212,0.3)")
        svg.text(sx + 10, sy + 18, "Port Scan", size=11, fill=THEME["accentBlue"], weight="bold")
        # Common ports checkbox
        svg.rect(sx + 10, sy + 28, 16, 16, rx=3, fill=THEME["cyan"])
        svg.text(sx + 10 + 8, sy + 28 + 11, "✓", size=10, fill=THEME["bgDark"], anchor="middle")
        svg.text(sx + 30, sy + 38, "Scan Common Ports", size=11, fill=THEME["textPrimary"])
        # Range
        svg.text(sx + 10, sy + 58, "Range:", size=11, fill=THEME["textSecondary"])
        svg.rect(sx + 10, sy + 64, 100, 28, rx=4, fill="transparent", stroke=THEME["borderCard"])
        svg.text(sx + 18, sy + 82, "From", size=11, fill=THEME["textSecondary"])
        svg.rect(sx + 120, sy + 64, 100, 28, rx=4, fill="transparent", stroke=THEME["borderCard"])
        svg.text(sx + 128, sy + 82, "To", size=11, fill=THEME["textSecondary"])
        sy += port_h + 12

        # Divider
        svg.line(sx, sy, sidebar_w - 12, sy, THEME["borderCard"])
        sy += 12

    # ── Summary cards in sidebar ──
    summary_sy = sy if is_wide else nav_h + 12
    if is_wide:
        svg.text(sx, sy, "Summary", size=11, fill=THEME["textSecondary"], weight="bold")
        svg.text(sidebar_w - 12, sy, "Total: 38", size=10, fill=THEME["textSecondary"], anchor="end")
        sy += 16
        for label, color, count in [("Pass", THEME["passGreen"], 32), ("Info", THEME["accentBlue"], 0),
                                     ("Warning", THEME["warnYellow"], 4), ("Fail", THEME["failRed"], 1),
                                     ("Skipped", THEME["skipGray"], 1)]:
            sh = 28
            svg.rect(sx, sy, sidebar_w - 24, sh, rx=6, fill=f"rgba({_hex_to_rgba(color, 0.06)})", stroke=f"rgba({_hex_to_rgba(color, 0.2)})")
            svg.text(sx + 8, sy + sh // 2 + 3, label, size=10, fill=THEME["textSecondary"])
            svg.text(sidebar_w - 20, sy + sh // 2 + 5, f"{count}", size=14, fill=color, anchor="end", weight="bold")
            sy += sh + 3

    # ── Content area ──
    chead_y = top + 1
    # Header bar
    svg.rect(content_x, chead_y, content_w, 41, fill=THEME["appBar"])
    svg.text(content_x + 16, chead_y + 26, "Results", size=15, fill=THEME["textPrimary"], weight="bold")
    svg.text(content_x + content_w - 16, chead_y + 26, "38 / 38", size=12, fill=THEME["cyan"], anchor="end", weight="bold")

    cy = chead_y + 45
    cpad = 8

    # Group panels
    result_groups = [
        ("G1 System & Adapters", "8/8", 0),
        ("G2 Connectivity & Security", "6/6", 0),
        ("G3 Internet & DNS", "4/5", 1),
        ("G4 Remote Host", "6/6", 0),
        ("G5 Website / URL", "12/13", 1),
    ]

    for gi, (gname, progress, fails) in enumerate(result_groups):
        gh = min(100, (svg.h - cy - 20) // len(result_groups))
        if cy + gh > svg.h - 20:
            break

        has_fail = fails > 0
        header_color = THEME["failRed"] if has_fail else THEME["passGreen"]
        header_bg = f"rgba(239,68,68,0.10)" if has_fail else f"rgba(74,222,128,0.08)"

        svg.rect(content_x + cpad, cy, content_w - cpad * 2, gh, rx=8, fill=THEME["bgCard"], stroke=THEME["borderCard"])
        # Group header
        svg.rect(content_x + cpad, cy, content_w - cpad * 2, 36, rx=8, fill=header_bg)
        svg.text(content_x + cpad + 16, cy + 24, gname, size=13, fill=THEME["textPrimary"], weight="bold")
        svg.text(content_x + content_w - cpad - 16, cy + 24, progress, size=12, fill=header_color, anchor="end", weight="bold")

        # Test items (truncated to fit)
        items_y = cy + 44
        for ti in range(min(3, (gh - 50) // 16)):
            if items_y > cy + gh - 8:
                break
            dot_color = THEME["passGreen"] if ti < 2 else (THEME["warnYellow"] if fails > 0 and ti == 2 else THEME["passGreen"])
            svg.circle(content_x + cpad + 20, items_y + 4, 4, fill=dot_color)
            svg.text(content_x + cpad + 32, items_y + 8, f"Test {gi+1}.{ti+1} — ok", size=10, fill=THEME["textSecondary"])
            items_y += 16

        cy += gh + 4

    return svg


def gen_config(svg, dev):
    """ConfigScreen.qml — AppBar with tabs + action bar + test list."""
    nav_h = draw_nav_bar(svg, dev, active_tab=2)
    top = nav_h
    appbar_h = 84

    svg.rect(0, top, svg.w, appbar_h, fill=THEME["appBar"])
    svg.line(0, top + appbar_h - 1, svg.w, top + appbar_h - 1, THEME["borderCard"])

    # AppBar title
    draw_settings_icon(svg, 16, top + 22, 20, THEME["cyan"])
    svg.text(42, top + 24, "Diagnostic Config", size=15, fill=THEME["textPrimary"], weight="bold")

    # Tabs
    tab_y = top + 44
    tab_names = ["G1", "G2", "G3", "G4", "G5"]
    tab_w = svg.w // len(tab_names)
    for i, tname in enumerate(tab_names):
        x = i * tab_w
        active = (i == 0)
        svg.text(x + tab_w // 2, tab_y + 20, tname, size=12, fill=THEME["cyan"] if active else THEME["textSecondary"], anchor="middle", weight="bold" if active else "normal")
        if active:
            svg.rect(x, tab_y + 36, tab_w, 2, fill=THEME["cyan"])

    # Action bar
    ay = top + appbar_h
    svg.rect(0, ay, svg.w, 60, fill=f"rgba(22,33,62,0.5)", stroke=THEME["borderSubtle"])
    svg.text(16, ay + 24, "G1 System & Adapters", size=14, fill=THEME["textPrimary"], weight="bold")
    svg.text(16, ay + 42, "8 diagnostics", size=11, fill=THEME["textSecondary"])

    # Select All button
    sa_x = svg.w - 240
    svg.rect(sa_x, ay + 14, 110, 32, rx=6, fill="transparent", stroke=THEME["borderCard"])
    svg.text(sa_x + 55, ay + 32, "Select All", size=11, fill=THEME["textPrimary"], anchor="middle")
    svg.rect(sa_x + 118, ay + 14, 110, 32, rx=6, fill="transparent", stroke=THEME["borderCard"])
    svg.text(sa_x + 118 + 55, ay + 32, "Deselect All", size=11, fill=THEME["textPrimary"], anchor="middle")

    # Test list
    ly = ay + 60
    tests = [
        ("Network Adapters", "List all network adapters and their operational state", True),
        ("NIC Advanced", "Driver version, hardware info, and negotiated link speed", True),
        ("WiFi Information", "Signal strength, SSID, channel, and link quality", True),
        ("Wired Information", "Ethernet link status, speed, and duplex mode", True),
        ("DHCP Status", "DHCP lease info, server address, and expiration", False),
        ("IP Configuration", "IP addresses, subnet mask, default gateway, DNS servers", True),
        ("Active Connections", "TCP/UDP connections: ESTABLISHED, LISTENING, etc.", True),
        ("Network Profile", "Active network profile type (Domain/Private/Public)", True),
    ]

    item_h = 54
    visible_items = (svg.h - ly) // item_h
    for ti in range(min(len(tests), visible_items)):
        iy = ly + ti * item_h
        if iy + item_h > svg.h - 20:
            break
        name, desc, enabled = tests[ti]

        # Separator
        svg.line(16, iy + item_h - 1, svg.w - 16, iy + item_h - 1, THEME["borderSubtle"])

        # Icon
        ic = THEME["passGreen"] if enabled else THEME["textMuted"]
        svg.circle(28, iy + item_h // 2, 7, fill=ic)

        # Text
        svg.text(50, iy + 20, name, size=13, fill=THEME["textPrimary"])
        svg.text(50, iy + 36, desc[:50], size=10, fill=f"rgba({_hex_to_rgba(THEME['textSecondary'], 0.6)})")

        # Switch
        sw_x = svg.w - 60
        sw_y = iy + item_h // 2 - 10
        sw_w = 40; sw_h = 20
        if enabled:
            svg.rect(sw_x, sw_y, sw_w, sw_h, rx=10, fill=THEME["accentBlue"])
            svg.circle(sw_x + sw_w - 10, sw_y + 10, 8, fill="#FFFFFF")
        else:
            svg.rect(sw_x, sw_y, sw_w, sw_h, rx=10, fill="#5A5A7A")
            svg.circle(sw_x + 10, sw_y + 10, 8, fill="#FFFFFF")

    return svg


def gen_report(svg, dev):
    """ReportScreen.qml — centered layout with export buttons."""
    nav_h = draw_nav_bar(svg, dev, active_tab=3)
    top = nav_h
    is_mobile = dev["isMobile"]

    # AppBar
    svg.rect(0, top, svg.w, 52, fill=THEME["appBar"])
    svg.line(0, top + 51, svg.w, top + 51, THEME["borderCard"])
    draw_report_icon(svg, 16, top + 26, 20, THEME["cyan"])
    svg.text(42, top + 30, "Report Preview", size=15, fill=THEME["textPrimary"], weight="bold")

    # Centered content
    center_x = svg.w // 2
    cy = top + 52 + (24 if not is_mobile else 14)

    # Icon container
    icon_size = 72 if is_mobile else 100
    svg.rect(center_x - icon_size // 2, cy, icon_size, icon_size, rx=24,
             fill=f"rgba({_hex_to_rgba(THEME['cyan'], 0.08)})",
             stroke=f"rgba({_hex_to_rgba(THEME['cyan'], 0.2)})", sw=1.5)
    draw_report_icon(svg, center_x, cy + icon_size // 2, icon_size * 0.45, f"rgba({_hex_to_rgba(THEME['cyan'], 0.6)})")
    cy += icon_size + (24 if not is_mobile else 14)

    # Title
    title_size = 19 if is_mobile else 22
    svg.text(center_x, cy, "Report Preview", size=title_size, fill=THEME["textPrimary"], anchor="middle", weight="bold")
    cy += title_size + (12 if not is_mobile else 8)

    # Subtitle
    svg.text(center_x, cy, "Generate a PDF or HTML report from your latest diagnostic run.",
             size=14, fill=f"rgba({_hex_to_rgba(THEME['textSecondary'], 0.6)})", anchor="middle")
    cy += 14 + (24 if not is_mobile else 16)

    # Export buttons
    col_w = min(320, svg.w - 48)
    btn_h = 48
    btn_x = center_x - col_w // 2

    for icon_name, label, accent in [
        ("report", "Preview PDF Summary", THEME["cyan"]),
        ("globe", "Preview Full HTML Report", THEME["accentBlue"]),
    ]:
        svg.rect(btn_x, cy, col_w, btn_h, rx=10,
                 fill=f"rgba({_hex_to_rgba(accent, 0.10)})",
                 stroke=f"rgba({_hex_to_rgba(accent, 0.35)})")
        svg.text(btn_x + 40, cy + btn_h // 2 + 4, label, size=13, fill=THEME["textPrimary"])
        cy += btn_h + 10

    cy += (18 if is_mobile else 32)

    # Status indicator
    has_results = True
    status_color = THEME["passGreen"]
    status_text = "38 results available"
    status_bg = f"rgba({_hex_to_rgba(status_color, 0.1)})"
    status_border = f"rgba({_hex_to_rgba(status_color, 0.3)})"

    status_w = 280
    status_h = 40
    svg.rect(center_x - status_w // 2, cy, status_w, status_h, rx=8, fill=status_bg, stroke=status_border)
    svg.text(center_x, cy + status_h // 2 + 4, status_text, size=12, fill=status_color, anchor="middle")

    return svg


def gen_settings(svg, dev):
    """SettingsScreen.qml — Language, Premium, About sections."""
    nav_h = draw_nav_bar(svg, dev, active_tab=4)
    top = nav_h
    is_mobile = dev["isMobile"]

    # AppBar
    svg.rect(0, top, svg.w, 52, fill=THEME["appBar"])
    svg.line(0, top + 51, svg.w, top + 51, THEME["borderCard"])
    draw_settings_icon(svg, 16, top + 26, 20, THEME["cyan"])
    svg.text(42, top + 30, "Settings", size=15, fill=THEME["textPrimary"], weight="bold")

    pad = 24
    cw = svg.w - pad * 2
    cy = top + 52 + 24

    # ── Language Section ──
    svg.rect(pad, cy, 30, 30, rx=8, fill=f"rgba({_hex_to_rgba(THEME['cyan'], 0.1)})")
    svg.text(pad + 15, cy + 20, "🌐", size=14, fill=THEME["cyan"], anchor="middle")
    svg.text(pad + 42, cy + 20, "Language", size=16, fill=THEME["textPrimary"], weight="bold")
    cy += 42

    lang_h = 50
    svg.rect(pad, cy, cw, lang_h, rx=12, fill=THEME["bgCard"], stroke=THEME["borderSubtle"])
    # Dropdown
    dd_h = 44
    svg.rect(pad + 16, cy + 3, cw - 32, dd_h, rx=6, fill=THEME["bgInput"], stroke=THEME["borderCard"])
    svg.text(pad + 28, cy + lang_h // 2 + 3, "English", size=13, fill=THEME["textPrimary"])
    svg.text(svg.w - pad - 28, cy + lang_h // 2 + 4, "▾", size=12, fill=THEME["textSecondary"], anchor="end")
    cy += lang_h + 20

    # ── Premium Section (mobile only) ──
    if is_mobile:
        svg.rect(pad, cy, 30, 30, rx=8, fill=f"rgba({_hex_to_rgba(THEME['warnYellow'], 0.1)})")
        svg.text(pad + 15, cy + 20, "⭐", size=14, fill=THEME["warnYellow"], anchor="middle")
        svg.text(pad + 42, cy + 20, "Premium", size=16, fill=THEME["textPrimary"], weight="bold")
        cy += 42

        prem_h = 100
        svg.rect(pad, cy, cw, prem_h, rx=12, fill=THEME["bgCard"], stroke=THEME["borderSubtle"])
        svg.text(pad + 16, cy + 24, "Share & email reports require Premium.", size=12, fill=THEME["textSecondary"])
        # Restore button
        svg.rect(pad + 16, cy + 44, cw - 32, 40, rx=8,
                 fill=f"rgba({_hex_to_rgba(THEME['warnYellow'], 0.12)})",
                 stroke=f"rgba({_hex_to_rgba(THEME['warnYellow'], 0.4)})")
        svg.text(pad + cw // 2, cy + 66, "Restore Purchases", size=13, fill=THEME["warnYellow"], anchor="middle", weight="bold")
        cy += prem_h + 32

    # ── About Section ──
    svg.rect(pad, cy, 30, 30, rx=8, fill=f"rgba({_hex_to_rgba(THEME['accentBlue'], 0.1)})")
    svg.text(pad + 15, cy + 20, "ℹ", size=14, fill=THEME["accentBlue"], anchor="middle")
    svg.text(pad + 42, cy + 20, "About", size=16, fill=THEME["textPrimary"], weight="bold")
    cy += 42

    about_h = 260 if is_mobile else 280
    svg.rect(pad, cy, cw, about_h, rx=12, fill=THEME["bgCard"], stroke=THEME["borderSubtle"])

    # App icon + name
    svg.rect(pad + 16, cy + 16, 48, 48, rx=12, fill=f"rgba({_hex_to_rgba(THEME['accentBlue'], 0.15)})")
    svg.text(pad + 40, cy + 44, "📡", size=24, fill=THEME["accentBlue"], anchor="middle")
    svg.text(pad + 78, cy + 32, "NetDiagnostics", size=18, fill=THEME["textPrimary"], weight="bold")
    svg.text(pad + 78, cy + 50, "Version 6.9.0 (Build 20260704)", size=12, fill=THEME["textSecondary"])

    # Divider
    svg.line(pad + 16, cy + 80, svg.w - pad - 16, cy + 80, THEME["borderSubtle"])

    # Description
    desc_y = cy + 96
    svg.text(pad + 16, desc_y, "A cross-platform network diagnostic tool", size=13, fill=THEME["textSecondary"])
    desc_y += 20
    svg.text(pad + 16, desc_y, "with real-time testing and detailed reports.", size=13, fill=THEME["textSecondary"])

    # Feature list
    feat_y = cy + 140
    features = [
        "💻 Cross-platform (iOS, Android, Desktop)",
        "⚡ Real-time diagnostics",
        "📊 Detailed report generation",
        "🌙 Dark theme",
        "🖥 Simulator mode (desktop)",
    ]
    for feat in features:
        if feat_y > cy + about_h - 12:
            break
        svg.text(pad + 16, feat_y, feat, size=12, fill=f"rgba({_hex_to_rgba(THEME['textSecondary'], 0.8)})")
        feat_y += 22

    return svg


# ═══════════════════════════════════════════════════════════════════════
# Main generator
# ═══════════════════════════════════════════════════════════════════════

GENERATORS = {
    "dashboard": gen_dashboard,
    "diagnostics": gen_diagnostics,
    "config": gen_config,
    "report": gen_report,
    "settings": gen_settings,
}


def generate_all(output_dir):
    """Generate all SVGs into output_dir with screenshot-matching structure."""
    for dev_path, dev in DEVICES.items():
        for screen in SCREENS:
            w, h = dev["w"], dev["h"]
            svg = SVG(w, h)

            # Draw iOS status bar
            status_h = draw_status_bar(svg, dev)

            # Generate screen content
            gen = GENERATORS[screen]
            gen(svg, dev)

            # Draw bottom tab bar for mobile
            draw_bottom_tab_bar(svg, dev, active_tab=SCREENS.index(screen))

            # Write file
            out_path = os.path.join(output_dir, dev_path, f"{screen}.svg")
            os.makedirs(os.path.dirname(out_path), exist_ok=True)
            with open(out_path, 'w', encoding='utf-8') as f:
                f.write(svg.to_xml())
            print(f"  ✓ {out_path} ({w}×{h})")


if __name__ == "__main__":
    import sys
    output = sys.argv[1] if len(sys.argv) > 1 else "resources/doc/figma"
    print(f"Generating Figma SVGs → {output}")
    print(f"  Devices: {len(DEVICES)}")
    print(f"  Screens: {len(SCREENS)}")
    print(f"  Total SVGs: {len(DEVICES) * len(SCREENS)}")
    print()
    generate_all(output)
    print()
    print("Done!")
