#!/usr/bin/env python3
"""Generate accurate Figma SVGs from Qt6 QML source — v2 with 5-Why fixes.

Fixes applied (see 5-Why analysis):
  F1: Removed fake iOS status bar (QML uses FramelessWindowHint)
  F2: Removed fake bottom tab bar (QML uses top nav bar only)
  F3: Nav bar exact from AppContent.qml: compact=32px/44px tabs,
      desktop=36px/100px tabs, center-aligned, active bg no indicator line
  F4: Font: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"
  F5: Diagnostic sidebar: full SidebarContent tree (260px)
  F6: Dashboard: exact 24px padding (width-48, x=24 from QML)
  F7: Config: 84px AppBar (44 title + 38 tabs + 2 border)
  F8: DiagGroupPanel: 3×24 accent bar, badges, progress, tree connector
  F9: Wide breakpoint: width >= 600 (not isMobile)

Directory structure (matches screenshot/):
  figma/{os}/{size}/{device}/{screen}.svg
"""

import os, html, math

# ═══════════════════════════════════════════════════════════════
# Theme — exact from AppTheme.qml
# ═══════════════════════════════════════════════════════════════
T = {
    "bgDark": "#1E1E2E", "bgSidebar": "#252538", "bgCard": "#16213E",
    "bgInput": "#2A2A4A", "appBar": "#1A1A2E",
    "t1": "#E0E0E0", "t2": "#A0A0B8", "t3": "#606080",
    "accent": "#E94560", "blue": "#0078D4", "cyan": "#00BCD4",
    "green": "#4ADE80", "yellow": "#FACC15", "red": "#EF4444",
    "gray": "#888888",
    "border": "#3A3A5A", "border2": "#2A2A4A",
}
# Exact font stack from QML Labels
FONT = "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"

# ═══════════════════════════════════════════════════════════════
# Devices (logical pixels = physical / scale factor)
# ═══════════════════════════════════════════════════════════════
DEVICES = {
    "ios/phone/6.1":  {"w": 375, "h": 812, "scale": 3, "name": "iPhone 6.1″"},
    "ios/phone/6.3":  {"w": 402, "h": 874, "scale": 3, "name": "iPhone 6.3″"},
    "ios/phone/6.5":  {"w": 414, "h": 896, "scale": 3, "name": "iPhone 6.5″"},
    "ios/phone/6.9":  {"w": 440, "h": 956, "scale": 3, "name": "iPhone 6.9″"},
    "ios/tablet/13":  {"w": 1032, "h": 1376, "scale": 2, "name": "iPad 13″"},
}

SCREENS = ["dashboard", "diagnostics", "config", "report", "settings"]

# ═══════════════════════════════════════════════════════════════
# SVG Builder
# ═══════════════════════════════════════════════════════════════
class S:
    def __init__(s, w, h):
        s.W, s.H = w, h
        s.p = []
        s._n = 0
        s._defs = []

    def df(s, x): s._defs.append(x)

    def r(s, x, y, w, h, rx=0, f=None, st=None, sw=1, o=None):
        a = [f'x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"']
        if rx: a.append(f'rx="{rx}"')
        if f: a.append(f'fill="{f}"')
        if st: a.append(f'stroke="{st}" stroke-width="{sw}"')
        if o is not None: a.append(f'opacity="{o}"')
        s.p.append(f'<rect {" ".join(a)}/>')
        return s

    def t(s, x, y, txt, sz=12, clr=T["t1"], a="start", w="normal", mx=None):
        t = html.escape(str(txt))
        at = [f'font-family="{FONT}"', f'font-size="{sz}"', f'fill="{clr}"',
              f'text-anchor="{a}"', f'font-weight="{w}"']
        if mx: at.append(f'class="elide"')
        s.p.append(f'<text x="{x:.1f}" y="{y:.1f}" {" ".join(at)}>{t}</text>')
        return s

    def c(s, cx, cy, r, f=None, st=None, sw=1):
        a = [f'cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}"']
        if f: a.append(f'fill="{f}"')
        if st: a.append(f'stroke="{st}" stroke-width="{sw}"')
        s.p.append(f'<circle {" ".join(a)}/>')
        return s

    def ln(s, x1, y1, x2, y2, st=T["border"], sw=1):
        s.p.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{st}" stroke-width="{sw}"/>')
        return s

    def g(s, t=None, o=None):
        a = ""
        if t: a += f' transform="{t}"'
        if o is not None: a += f' opacity="{o}"'
        s.p.append(f'<g{a}>')
        return _G(s)

    def badge(s, x, y, count, color, w=24, h=17):
        """Dashboard-style badge pill."""
        if count <= 0: return 0
        s.r(x, y, w, h, rx=4, f=_alpha(color, 0.15))
        s.t(x + w/2, y + h/2 + 3.5, str(count), sz=10, clr=color, a="middle", w="bold")
        return w + 4

    def progress(s, x, y, w, h, pct, color):
        s.r(x, y, w, h, rx=h/2, f=T["border2"])
        if pct > 0:
            s.r(x, y, w * pct, h, rx=h/2, f=color)

    def switch(s, x, y, on):
        """iOS-style toggle switch."""
        sw_w, sw_h = 40, 22
        if on:
            s.r(x, y, sw_w, sw_h, rx=11, f=T["blue"])
            s.c(x + sw_w - 11, y + 11, 9, f="#FFFFFF")
        else:
            s.r(x, y, sw_w, sw_h, rx=11, f="#5A5A7A")
            s.c(x + 11, y + 11, 9, f="#FFFFFF")

    def checkbox(s, x, y, checked, sz=16):
        s.r(x, y, sz, sz, rx=3, f=T["blue"] if checked else "none", st=T["blue"] if checked else "#5A5A7A", sw=1.5)
        if checked:
            s.t(x+sz/2, y+sz/2+3, "✓", sz=10, clr="#FFFFFF", a="middle")

    def to_xml(s):
        lines = [f'<?xml version="1.0" encoding="UTF-8"?>',
                 f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {s.W} {s.H}" width="{s.W}" height="{s.H}">']
        if s._defs:
            lines.append('<defs>')
            lines.extend(s._defs)
            lines.append('</defs>')
            lines.append('<style>.elide{overflow:hidden;text-overflow:ellipsis;}</style>')
        lines.extend(s.p)
        lines.append('</svg>')
        return '\n'.join(lines)


class _G:
    def __init__(s, svg): s.s = svg
    def __enter__(s): return s.s
    def __exit__(s, *a): s.s.p.append('</g>')


def _alpha(hex_color, a):
    c = hex_color.lstrip('#')
    r, g, b = int(c[0:2],16), int(c[2:4],16), int(c[4:6],16)
    return f"rgba({r},{g},{b},{a})"


# ═══════════════════════════════════════════════════════════════
# F2+F3: Exact Nav Bar from AppContent.qml
# ═══════════════════════════════════════════════════════════════
def draw_nav_bar(s, W, compact, active_tab):
    """AppContent.qml nav bar — exact."""
    bar_h = 32 if compact else 36
    s.r(0, 0, W, bar_h, f=T["appBar"])
    # In QML the nav bar is part of ColumnLayout with no explicit border,
    # but it has a 1px separator at the bottom of the content area.

    TABS = [
        ("dashboard",  "⊞", "Dashboard"),
        ("diagnostic", "⚡", "Diag"),
        ("config",     "⚙", "Config"),
        ("report",     "📄", "Report"),
        ("settings",   "⚙", "Settings"),
    ]
    tab_w = 44 if compact else 100

    if compact:
        # Mobile: right-aligned tabs (from QML: Row { spacing: 0 })
        start_x = W - len(TABS) * tab_w - 4
    else:
        # Desktop: centered (RowLayout with spacer Items)
        start_x = (W - len(TABS) * tab_w) // 2

    for i, (screen, icon, label) in enumerate(TABS):
        x = start_x + i * tab_w
        active = (i == active_tab)
        # Background on active
        if active:
            s.r(x, bar_h/2 - 16, tab_w, 32, rx=6, f=_alpha(T["cyan"], 0.12))
        if compact:
            # Icon only, centered
            s.t(x + tab_w/2, bar_h/2 + 3, icon, sz=14,
                clr=T["cyan"] if active else _alpha(T["t1"], 0.55), a="middle")
        else:
            # Icon + label
            ic_x = x + 8
            s.t(ic_x + 6, bar_h/2 + 3, icon, sz=12,
                clr=T["cyan"] if active else _alpha(T["t1"], 0.55), a="middle")
            s.t(ic_x + 18, bar_h/2 + 3, label, sz=10,
                clr=T["cyan"] if active else _alpha(T["t1"], 0.7),
                w="bold" if active else "normal")

    # Close button — desktop only
    if not compact:
        cx = W - 22
        s.r(cx - 14, 4, 28, 28, rx=6, f="transparent", st="#5A5A7A")
        s.t(cx, bar_h/2 + 3, "✕", sz=12, clr=_alpha(T["t1"], 0.7), a="middle")

    return bar_h


# ═══════════════════════════════════════════════════════════════
# F5: Diagnostic Sidebar — full SidebarContent from DiagnosticScreen.qml
# ═══════════════════════════════════════════════════════════════
def draw_diag_sidebar(s, W, H, top, sidebar_w, compact):
    """Full SidebarContent component from DiagnosticScreen.qml."""
    sw = sidebar_w
    sx = 12  # left/right margin inside sidebar

    # Sidebar bg
    s.r(0, top, sw, H - top, f=T["bgSidebar"])

    cy = top

    # Header — Rectangle with border bottom
    s.r(0, cy, sw, 48, f="transparent", st=T["border"])
    s.t(16, cy + 30, "NetDiagnostics", sz=16, clr=T["t1"], w="bold")
    cy += 60

    if compact:
        # Compact mode: only target input + run button visible
        # Target input
        s.t(sx, cy, "Target", sz=11, clr=T["t2"], w="bold")
        cy += 18
        s.r(sx, cy, sw - 24, 40, rx=8, f=T["bgInput"], st=T["border"])
        s.t(sx + 10, cy + 25, "Enter URL, IP address, or hostname...", sz=11,
            clr=_alpha(T["t2"], 0.4))
        cy += 48

        # Run button
        s.r(sx, cy, sw - 72, 38, rx=8, f=T["blue"])
        s.t(sx + (sw - 72)/2, cy + 25, "▶ Run Diagnostics", sz=12, clr="#FFFFFF", a="middle", w="bold")
        # Stop button placeholder
        s.r(sx + sw - 66, cy, 42, 38, rx=8, f="transparent", st=_alpha(T["red"], 0.5))
        s.t(sx + sw - 45, cy + 25, "■", sz=12, clr=T["red"], a="middle")
        return cy + 50

    # Full sidebar content
    # Target input
    s.t(sx, cy, "Target", sz=11, clr=T["t2"], w="bold")
    cy += 18
    s.r(sx, cy, sw - 24, 40, rx=8, f=T["bgInput"], st=T["border"])
    s.t(sx + 10, cy + 25, "Enter URL, IP address, or hostname...", sz=11,
        clr=_alpha(T["t2"], 0.4))
    cy += 48

    # Run button + stop
    s.r(sx, cy, sw - 72, 38, rx=8, f=T["blue"])
    s.t(sx + (sw - 72)/2, cy + 25, "▶ Run Diagnostics", sz=12, clr="#FFFFFF", a="middle", w="bold")
    s.r(sx + sw - 66, cy, 42, 38, rx=8, f="transparent", st=_alpha(T["red"], 0.5))
    s.t(sx + sw - 45, cy + 25, "■", sz=12, clr=T["red"], a="middle")
    cy += 46

    # "Diagnosis Group" label
    s.t(sx, cy, "Diagnosis Group", sz=11, clr=T["t2"], w="bold")
    cy += 12

    # G1-G5 checkboxes
    groups = [
        ("G1 System & Adapters", True),
        ("G2 Connectivity & Security", True),
        ("G3 Internet & DNS", True),
        ("G4 Remote Host", False),
        ("G5 Website / URL", False),
    ]
    for gname, checked in groups:
        s.r(sx, cy, sw - 24, 32, rx=6, f=_alpha(T["blue"], 0.12) if checked else "transparent")
        s.checkbox(sx + 8, cy + 8, checked)
        s.t(sx + 32, cy + 20, gname, sz=12,
            clr=T["t1"] if checked else T["t2"])
        cy += 34

    cy += 2

    # PortScanConfig card
    port_h = 106
    s.r(sx, cy, sw - 24, port_h, rx=8, f=_alpha(T["bgCard"], 0.5),
        st=_alpha(T["blue"], 0.3))
    s.t(sx + 10, cy + 16, "Port Scan", sz=11, clr=T["blue"], w="bold")
    # Common ports checkbox
    s.checkbox(sx + 10, cy + 26, True)
    s.t(sx + 34, cy + 36, "Scan Common Ports", sz=11, clr=T["t1"])
    # Range
    s.t(sx + 10, cy + 56, "Range:", sz=11, clr=T["t2"])
    s.r(sx + 10, cy + 62, 96, 28, rx=4, f="transparent", st=T["border"])
    s.t(sx + 18, cy + 80, "From", sz=11, clr=T["t2"])
    s.t(sx + 114, cy + 80, "–", sz=13, clr=T["t2"])
    s.r(sx + 126, cy + 62, 96, 28, rx=4, f="transparent", st=T["border"])
    s.t(sx + 134, cy + 80, "To", sz=11, clr=T["t2"])
    cy += port_h + 8

    # Divider
    s.r(sx, cy, sw - 24, 20, f="transparent")
    s.ln(sx, cy + 10, sw - sx, cy + 10, T["border"])
    cy += 20

    # TargetAnalysisPanel (if target non-empty)
    s.r(sx, cy, sw - 24, 70, rx=8, f=_alpha(T["bgCard"], 0.5), st=_alpha(T["blue"], 0.3))
    s.t(sx + 10, cy + 16, "Target Analysis", sz=11, clr=T["blue"], w="bold")
    s.t(sx + 10, cy + 34, "Type    : IPv4 Address", sz=10, clr=T["t2"])
    s.t(sx + 10, cy + 50, "Host    : 192.168.1.1", sz=10, clr=T["t1"])
    s.t(sx + 10, cy + 64, "RFC1918 Private", sz=10, clr=T["t2"])
    cy += 80

    # Spacer
    # (QML: Item { Layout.fillHeight: true })

    # SummaryCards at bottom
    sum_y = max(cy, H - 180)
    s.r(0, sum_y - 8, sw, H - sum_y + 8, f="transparent", st=T["border"])
    s.t(sx, sum_y + 12, "Summary", sz=11, clr=T["t2"], w="bold")
    s.t(sw - sx, sum_y + 12, "Total: 38", sz=10, clr=T["t2"], a="end")
    sum_cy = sum_y + 24
    for label, color, count in [("Pass", T["green"], 32), ("Warning", T["yellow"], 4),
                                 ("Fail", T["red"], 1), ("Skipped", T["gray"], 1)]:
        sh = 26
        s.r(sx, sum_cy, sw - 24, sh, rx=6, f=_alpha(color, 0.06), st=_alpha(color, 0.2))
        s.t(sx + 8, sum_cy + sh/2 + 3, label, sz=10, clr=_alpha(T["t2"], 0.8))
        s.t(sw - 16, sum_cy + sh/2 + 5, f"{count:2d}", sz=14, clr=color, a="end", w="bold")
        sum_cy += sh + 3

    return H  # sidebar fills entire height


# ═══════════════════════════════════════════════════════════════
# Screen generators
# ═══════════════════════════════════════════════════════════════

def gen_dashboard(s, W, H, compact):
    """DashboardScreen.qml — F6: exact 24px padding."""
    top = draw_nav_bar(s, W, compact, active_tab=0)

    # AppBar — 52px
    appbar_h = 52
    s.r(0, top, W, appbar_h, f=T["appBar"])
    s.ln(0, top + appbar_h - 1, W, top + appbar_h - 1, T["border"])
    s.t(16, top + appbar_h/2 + 4, "Dashboard", sz=15, clr=T["t1"], w="bold")

    # Reset button
    s.r(W - 76, top + 10, 60, 32, rx=6, f="transparent", st="#5A5A7A")
    s.t(W - 46, top + appbar_h/2 + 4, "Reset", sz=12, clr=T["t2"], a="middle")

    cy = top + appbar_h + 24  # QML: Item { Layout.preferredHeight: 24 }
    pad = 24  # F6: exact from QML width: parent.width-48, x: 24
    cw = W - pad * 2

    # ── Run Info Header Card ──
    card_h = 80
    s.r(pad, cy, cw, card_h, rx=12, f=T["bgCard"], st=T["border2"])
    s.t(pad + 16, cy + 24, "Diagnostic Run Complete", sz=16, clr=T["t1"], w="bold")
    s.t(pad + 16, cy + 46, "Target: 192.168.1.1", sz=12, clr=T["t2"])
    s.t(pad + 16, cy + 64, "14:32:18", sz=12, clr=T["t2"])
    cy += card_h + 24

    # ── Summary Cards (SummaryCards.qml inline) ──
    s.t(pad, cy, "Summary", sz=11, clr=T["t2"], w="bold")
    s.t(W - pad, cy, "Total: 38", sz=10, clr=T["t2"], a="end")
    cy += 16
    for label, color, count in [("Pass", T["green"], 32), ("Info", T["blue"], 0),
                                 ("Warning", T["yellow"], 4), ("Fail", T["red"], 1),
                                 ("Skipped", T["gray"], 1)]:
        sh = 32
        s.r(pad, cy, cw, sh, rx=6, f=_alpha(color, 0.06), st=_alpha(color, 0.2))
        s.t(pad + 8, cy + sh/2 + 3, label, sz=10, clr=_alpha(T["t2"], 0.8))
        s.t(W - pad - 8, cy + sh/2 + 5, f"{count:3d}", sz=16, clr=color, a="end", w="bold")
        cy += sh + 4
    cy += 16

    # ── Per-Group Results ──
    s.t(pad, cy, "Per-Group Results", sz=15, clr=T["t1"], w="bold")
    cy += 22

    groups = [
        ("G1: System & Adapters", 8, 8, 0, 0, 0, 0.95, "2.1s"),
        ("G2: Connectivity & Security", 6, 6, 0, 0, 0, 0.85, "1.8s"),
        ("G3: Internet & DNS", 5, 4, 0, 1, 0, 0.65, "3.4s"),
        ("G4: Remote Host", 6, 6, 0, 0, 0, 0.92, "2.2s"),
        ("G5: Website / URL", 13, 8, 0, 3, 1, 0.78, "5.1s"),
    ]

    for gname, total, p, w, f, sk, pct, dur in groups:
        gh = 90
        s.r(pad, cy, cw, gh, rx=10, f=T["bgCard"], st=T["border2"])
        # F8: accent bar 3×24 (reduced in compact for space)
        bar_x = pad + 14
        s.r(bar_x, cy + 12, 3, 24, rx=2, f=T["blue"])
        # Group name
        s.t(bar_x + 11, cy + 18, gname, sz=13, clr=T["t1"], w="bold")

        # Badges from right
        bx = W - pad - 16
        for cnt, clr in [(sk, T["gray"]), (f, T["red"]), (w, T["yellow"]), (p, T["green"])]:
            if cnt > 0:
                bw = 24; bh = 17
                bx -= bw + 4
                s.r(bx, cy + 11, bw, bh, rx=4, f=_alpha(clr, 0.15))
                s.t(bx + bw/2, cy + 11 + bh/2 + 3, str(cnt), sz=10, clr=clr, a="middle", w="bold")

        # Duration
        s.t(W - pad - 16, cy + 32, dur, sz=11, clr=T["t2"], a="end")

        # F8: Progress bar
        pr_y = cy + 44
        s.r(bar_x, pr_y, cw - 28, 4, rx=2, f=T["border2"])
        color = T["red"] if f > 0 else (T["yellow"] if w > 0 else T["green"])
        s.r(bar_x, pr_y, (cw - 28) * pct, 4, rx=2, f=color)

        # Tree connector + test items
        ti_y = cy + 56
        for ti in range(min(3, total)):
            if ti_y > cy + gh - 8: break
            # Tree connector vertical line
            s.r(bar_x + 7, ti_y - 8, 2, 16 - 8, f=T["border2"])
            s.c(bar_x + 12, ti_y, 4, f=T["green"] if ti < (total - f) else (T["yellow"] if ti < (total - f + w) else T["red"]))
            s.t(bar_x + 20, ti_y + 3, f"Test {ti+1}.{'pass' if ti < (total-f-w) else 'warn' if ti < (total-f) else 'fail'}", sz=10, clr=T["t2"])
            ti_y += 14

        cy += gh + 8

    # ── Overall Summary ──
    cy += 4
    s2_h = 150
    s.r(pad, cy, cw, s2_h, rx=12, f=T["bgCard"], st=T["border2"])
    s.t(pad + 16, cy + 22, "Summary", sz=15, clr=T["t1"], w="bold")
    iy = cy + 50
    for lbl, val, clr in [("Total Diagnostics", "38", T["cyan"]),
                           ("Total Time", "12.4s", T["blue"]),
                           ("Completed", "38", T["green"])]:
        s.t(pad + 16, iy, lbl, sz=12, clr=T["t2"])
        s.t(W - pad - 16, iy + 4, val, sz=16, clr=clr, a="end", w="bold")
        iy += 24
    s.ln(pad + 16, iy, W - pad - 16, iy, T["border2"])
    iy += 8
    s.t(pad + 16, iy + 8, "Layer Timings", sz=12, clr=T["t2"], w="bold")
    iy += 20
    for gi, gn in enumerate(["G1 System & Adapters", "G2 Connectivity", "G3 Internet & DNS",
                              "G4 Remote Host", "G5 Website / URL"]):
        if iy + 16 > cy + s2_h - 4: break
        s.r(pad + 16, iy, 8, 8, rx=2, f=T["blue"])
        s.t(pad + 30, iy + 8, gn, sz=11, clr=T["t1"])
        s.t(W - pad - 16, iy + 8, f"{2.0 + gi*0.8:.1f}s", sz=11, clr=T["t2"], a="end")
        iy += 18

    return s


def gen_diagnostics(s, W, H, compact):
    """DiagnosticScreen.qml — F5+F9: full sidebar, 600px breakpoint."""
    top = draw_nav_bar(s, W, compact, active_tab=1)

    # F9: wide detection by screen width, not device type
    is_wide = W >= 600

    if is_wide:
        sidebar_w = 260
        draw_diag_sidebar(s, W, H, top, sidebar_w, compact=False)
        content_x = sidebar_w + 1
        content_w = W - content_x
        # Divider
        s.ln(sidebar_w, top, sidebar_w, H, T["border"])
    else:
        sidebar_h = int((H - top) * 0.29)
        draw_diag_sidebar(s, W, top + sidebar_h + 1, top, W, compact=True)
        s.ln(0, top + sidebar_h, W, top + sidebar_h, T["border"])
        content_x = 0
        content_w = W
        top = top + sidebar_h + 1

    # ── Content area header ──
    s.r(content_x, top, content_w, 41, f=T["appBar"])
    s.t(content_x + 16, top + 26, "Results", sz=15, clr=T["t1"], w="bold")
    s.t(content_x + content_w - 16, top + 26, "38 / 38", sz=12, clr=T["cyan"], a="end", w="bold")

    cy = top + 45
    cpad = 8
    cw_content = content_w - cpad * 2

    # F8: DiagGroupPanel per group
    result_groups = [
        ("G1: System & Adapters", 8, 8, 0, 0, 0),
        ("G2: Connectivity & Security", 6, 6, 0, 0, 0),
        ("G3: Internet & DNS", 5, 4, 1, 0, 0),
        ("G4: Remote Host", 6, 6, 0, 0, 0),
        ("G5: Website / URL", 13, 8, 3, 1, 1),
    ]

    for gi, (gname, total, p, w, f, sk) in enumerate(result_groups):
        gh = min(95, max(70, (H - cy - 20) // len(result_groups)))
        if cy + gh >= H - 10: break

        s.r(content_x + cpad, cy, cw_content, gh, rx=10, f=T["bgCard"], st=T["border2"])

        # Header: accent bar + name + count + badges + expand arrow
        hdr_y = cy + 8
        s.r(content_x + cpad + 12, hdr_y, 3, 22, rx=2, f=T["blue"])
        s.t(content_x + cpad + 23, hdr_y + 4, gname, sz=13, clr=T["t1"], w="bold")
        # Count
        s.t(content_x + cpad + cw_content - 30, hdr_y + 14, f"{p}/{total}", sz=11, clr=T["t2"], a="end")
        # Expand arrow
        s.t(content_x + cpad + cw_content - 16, hdr_y + 4, "▼", sz=10, clr=T["t2"], a="end")

        # Badges
        bx = content_x + cpad + cw_content - 50
        for cnt, clr in [(sk, T["gray"]), (f, T["red"]), (w, T["yellow"]), (p, T["green"])]:
            if cnt > 0:
                bx -= 28
                s.r(bx, hdr_y, 26, 18, rx=4, f=_alpha(clr, 0.15))
                s.t(bx + 13, hdr_y + 12, str(cnt), sz=10, clr=clr, a="middle", w="bold")

        # Progress bar
        pct = p / total if total > 0 else 0
        pr_color = T["red"] if f > 0 else (T["yellow"] if w > 0 else T["green"])
        s.r(content_x + cpad + 12, cy + 38, cw_content - 24, 4, rx=2, f=T["border2"])
        s.r(content_x + cpad + 12, cy + 38, (cw_content - 24) * pct, 4, rx=2, f=pr_color)

        # Expanded tree items
        sep_y = cy + 46
        s.ln(content_x + cpad + 12, sep_y, content_x + cpad + cw_content - 12, sep_y, T["border2"])
        ti_y = sep_y + 4
        for ti in range(min(3, total)):
            if ti_y > cy + gh - 6: break
            # Tree connector vertical line
            s.r(content_x + cpad + 20, ti_y, 2, 12, f=T["border2"])
            status_color = T["red"] if ti >= total - f else (T["yellow"] if ti >= total - f - w else T["green"])
            s.c(content_x + cpad + 28, ti_y + 6, 4, f=status_color)
            s.t(content_x + cpad + 36, ti_y + 9, f"Test {gi+1}.{ti+1}", sz=10, clr=T["t2"])
            s.t(content_x + cpad + cw_content - 16, ti_y + 9, f"{10+ti*5}ms", sz=10, clr=_alpha(T["t2"], 0.6), a="end")
            ti_y += 14

        cy += gh + 4

    return s


def gen_config(s, W, H, compact):
    """ConfigScreen.qml — F7: 84px AppBar with tab row."""
    top = draw_nav_bar(s, W, compact, active_tab=2)

    # F7: AppBar 84px = 44 (title) + 38 (tabs) + 2
    appbar_h = 84
    s.r(0, top, W, appbar_h, f=T["appBar"])
    s.ln(0, top + appbar_h - 1, W, top + appbar_h - 1, T["border"])

    # Title row (44px)
    s.t(16, top + 26, "Diagnostic Config", sz=15, clr=T["t1"], w="bold")

    # Tab row (38px from y=44)
    tab_y = top + 44
    tab_names = ["G1", "G2", "G3", "G4", "G5"]
    tab_w = W // len(tab_names)
    for i, tn in enumerate(tab_names):
        x = i * tab_w
        active = (i == 0)
        s.t(x + tab_w/2, tab_y + 22, tn, sz=12,
            clr=T["cyan"] if active else T["t2"], a="middle",
            w="bold" if active else "normal")
        if active:
            s.r(x, tab_y + 36, tab_w, 2, f=T["cyan"])

    # Action bar
    ay = top + appbar_h
    s.r(0, ay, W, 60, f=_alpha(T["bgCard"], 0.5), st=T["border2"])
    s.t(16, ay + 24, "G1 System & Adapters", sz=14, clr=T["t1"], w="bold")
    s.t(16, ay + 44, "8 diagnostics", sz=11, clr=T["t2"])

    # Select All / Deselect All buttons
    sa_x = W - 244
    s.r(sa_x, ay + 14, 110, 32, rx=6, f="transparent", st=T["border"])
    s.t(sa_x + 55, ay + 32, "Select All", sz=11, clr=T["t1"], a="middle")
    s.r(sa_x + 118, ay + 14, 110, 32, rx=6, f="transparent", st=T["border"])
    s.t(sa_x + 118 + 55, ay + 32, "Deselect All", sz=11, clr=T["t1"], a="middle")

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
    for ti in range(min(len(tests), int((H - ly - 10) / item_h))):
        iy = ly + ti * item_h
        name, desc, enabled = tests[ti]

        # Separator
        s.ln(16, iy + item_h - 1, W - 16, iy + item_h - 1, T["border2"])

        # Leading icon
        s.c(28, iy + item_h/2, 7, f=T["green"] if enabled else T["t3"])

        # Text
        s.t(50, iy + 18, name, sz=13, clr=T["t1"])
        if desc:
            s.t(50, iy + 34, desc[:55] + ("..." if len(desc) > 55 else ""), sz=10,
                clr=_alpha(T["t2"], 0.6))

        # Switch
        s.switch(W - 60, iy + item_h/2 - 11, enabled)

    return s


def gen_report(s, W, H, compact):
    """ReportScreen.qml — centered layout."""
    top = draw_nav_bar(s, W, compact, active_tab=3)

    # AppBar 52px
    appbar_h = 52
    s.r(0, top, W, appbar_h, f=T["appBar"])
    s.ln(0, top + appbar_h - 1, W, top + appbar_h - 1, T["border"])
    s.t(16, top + appbar_h/2 + 4, "Report Preview", sz=15, clr=T["t1"], w="bold")

    cy = top + appbar_h + (14 if compact else 24)
    cx = W / 2

    # Icon container
    icon_sz = 72 if compact else 100
    s.r(cx - icon_sz/2, cy, icon_sz, icon_sz, rx=24,
        f=_alpha(T["cyan"], 0.08), st=_alpha(T["cyan"], 0.2), sw=1.5)
    s.t(cx, cy + icon_sz/2 + 5, "📄", sz=icon_sz*0.4, clr=_alpha(T["cyan"], 0.6), a="middle")
    cy += icon_sz + (14 if compact else 24)

    # Title
    title_sz = 19 if compact else 22
    s.t(cx, cy, "Report Preview", sz=title_sz, clr=T["t1"], a="middle", w="bold")
    cy += title_sz + (8 if compact else 12)

    # Subtitle
    s.t(cx, cy, "Generate a PDF or HTML report from your latest diagnostic run.",
        sz=14, clr=_alpha(T["t2"], 0.6), a="middle")
    cy += 14 + (16 if compact else 24)

    # Export buttons
    col_w = min(320, W - 48)
    btn_x = cx - col_w/2
    btn_h = 48

    for label, accent in [("Preview PDF Summary", T["cyan"]), ("Preview Full HTML Report", T["blue"])]:
        s.r(btn_x, cy, col_w, btn_h, rx=10, f=_alpha(accent, 0.10), st=_alpha(accent, 0.35))
        s.t(btn_x + 40, cy + btn_h/2 + 4, label, sz=13, clr=T["t1"])
        cy += btn_h + 10

    cy += (18 if compact else 32)

    # Status indicator
    status_h = 40
    s.r(cx - 140, cy, 280, status_h, rx=8, f=_alpha(T["green"], 0.1), st=_alpha(T["green"], 0.3))
    s.t(cx, cy + status_h/2 + 4, "38 results available", sz=12, clr=T["green"], a="middle")

    return s


def gen_settings(s, W, H, compact):
    """SettingsScreen.qml — language, premium, about."""
    top = draw_nav_bar(s, W, compact, active_tab=4)

    # AppBar 52px
    appbar_h = 52
    s.r(0, top, W, appbar_h, f=T["appBar"])
    s.ln(0, top + appbar_h - 1, W, top + appbar_h - 1, T["border"])
    s.t(16, top + appbar_h/2 + 4, "Settings", sz=15, clr=T["t1"], w="bold")

    pad = 24
    cw = W - pad * 2
    cy = top + appbar_h + 24

    # ── Language Section ──
    # Section header
    s.r(pad, cy, 30, 30, rx=8, f=_alpha(T["cyan"], 0.1))
    s.t(pad + 15, cy + 20, "🌐", sz=14, clr=T["cyan"], a="middle")
    s.t(pad + 42, cy + 20, "Language", sz=16, clr=T["t1"], w="bold")
    cy += 42

    # Dropdown card
    lang_h = 50
    s.r(pad, cy, cw, lang_h, rx=12, f=T["bgCard"], st=T["border2"])
    s.r(pad + 16, cy + 3, cw - 32, 44, rx=6, f=T["bgInput"], st=T["border"])
    s.t(pad + 28, cy + lang_h/2 + 3, "English", sz=13, clr=T["t1"])
    s.t(W - pad - 28, cy + lang_h/2 + 4, "▾", sz=12, clr=T["t2"], a="end")
    cy += lang_h + 20

    # ── Premium Section (compact/mobile only) ──
    if compact:
        s.r(pad, cy, 30, 30, rx=8, f=_alpha(T["yellow"], 0.1))
        s.t(pad + 15, cy + 20, "⭐", sz=14, clr=T["yellow"], a="middle")
        s.t(pad + 42, cy + 20, "Premium", sz=16, clr=T["t1"], w="bold")
        cy += 42

        prem_h = 100
        s.r(pad, cy, cw, prem_h, rx=12, f=T["bgCard"], st=T["border2"])
        s.t(pad + 16, cy + 22, "Share & email reports require Premium.", sz=12, clr=T["t2"])
        s.r(pad + 16, cy + 44, cw - 32, 40, rx=8,
            f=_alpha(T["yellow"], 0.12), st=_alpha(T["yellow"], 0.4))
        s.t(W/2, cy + 66, "Restore Purchases", sz=13, clr=T["yellow"], a="middle", w="bold")
        cy += prem_h + 32

    # ── About Section ──
    s.r(pad, cy, 30, 30, rx=8, f=_alpha(T["blue"], 0.1))
    s.t(pad + 15, cy + 20, "ℹ", sz=14, clr=T["blue"], a="middle")
    s.t(pad + 42, cy + 20, "About", sz=16, clr=T["t1"], w="bold")
    cy += 42

    about_h = 280 if compact else 260
    if cy + about_h > H - 10:
        about_h = H - cy - 10

    s.r(pad, cy, cw, about_h, rx=12, f=T["bgCard"], st=T["border2"])

    # App icon + name
    s.r(pad + 16, cy + 16, 48, 48, rx=12, f=_alpha(T["blue"], 0.15))
    s.t(pad + 40, cy + 44, "📡", sz=24, clr=T["blue"], a="middle")
    s.t(pad + 78, cy + 32, "NetDiagnostics", sz=18, clr=T["t1"], w="bold")
    s.t(pad + 78, cy + 50, "Version 6.9.0 (Build 20260704)", sz=12, clr=T["t2"])

    # Divider
    s.ln(pad + 16, cy + 80, W - pad - 16, cy + 80, T["border2"])

    # Description
    s.t(pad + 16, cy + 96, "A cross-platform network diagnostic tool", sz=13, clr=T["t2"])
    s.t(pad + 16, cy + 116, "with real-time testing and detailed reports.", sz=13, clr=T["t2"])

    # Features
    features = [
        "💻 Cross-platform (iOS, Android, Desktop)",
        "⚡ Real-time diagnostics",
        "📊 Detailed report generation",
        "🌙 Dark theme",
        "🖥 Simulator mode (desktop)",
    ]
    feat_y = cy + 144
    for feat in features:
        if feat_y > cy + about_h - 12: break
        s.t(pad + 16, feat_y, feat, sz=12, clr=_alpha(T["t2"], 0.8))
        feat_y += 22

    return s


# ═══════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════
GENERATORS = {
    "dashboard": gen_dashboard,
    "diagnostics": gen_diagnostics,
    "config": gen_config,
    "report": gen_report,
    "settings": gen_settings,
}


def generate_all(output_dir):
    for dev_path, dev in DEVICES.items():
        W, H = dev["w"], dev["h"]
        # F9: compact determined by screen width, not device.isMobile
        compact = W < 600
        for screen in SCREENS:
            svg = S(W, H)
            gen = GENERATORS[screen]
            gen(svg, W, H, compact)
            out_path = os.path.join(output_dir, dev_path, f"{screen}.svg")
            os.makedirs(os.path.dirname(out_path), exist_ok=True)
            with open(out_path, 'w', encoding='utf-8') as f:
                f.write(svg.to_xml())
            print(f"  ✓ {out_path} ({W}×{H}, {'compact' if compact else 'wide'})")


if __name__ == "__main__":
    import sys
    output = sys.argv[1] if len(sys.argv) > 1 else "resources/doc/figma"
    print(f"Generating Figma SVGs v2 → {output}")
    print(f"  {len(DEVICES)} devices × {len(SCREENS)} screens = {len(DEVICES)*len(SCREENS)} SVGs")
    print()
    generate_all(output)
    print(f"\nDone! {len(DEVICES)*len(SCREENS)} SVGs generated.")
