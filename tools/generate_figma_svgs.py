#!/usr/bin/env python3
"""
Figma SVG generator v3 — exact QML layout extraction.

Architecture:
  1. Exact layout parameters extracted from QML source (AppTheme, AppContent,
     each screen, each widget)
  2. Shared nav bar from AppContent.qml rendered once per screen
  3. Multi-OS: iOS/Android/Desktop with platform-appropriate behaviors
  4. Multi-size: phone (<600px compact) vs tablet/desktop (>=600px wide)

Devices match screenshot/ structure:
  ios/phone/6.1,6.3,6.5,6.9 | ios/tablet/13
  android/phone/6.1,6.3,6.5,6.9 | android/tablet/13
  desktop/wide/800,1024,1280,1440
"""

import os, html, math

# ═══════════════════════════════════════════════════════════════
# AppTheme.qml — exact 20-color token set
# ═══════════════════════════════════════════════════════════════
C = {
    "bgDark":      "#1E1E2E",
    "bgSidebar":   "#252538",
    "bgCard":      "#16213E",
    "bgInput":     "#2A2A4A",
    "appBar":      "#1A1A2E",
    "textPrimary": "#E0E0E0",
    "textSecondary":"#A0A0B8",
    "textMuted":   "#606080",
    "accent":      "#E94560",
    "blue":        "#0078D4",
    "cyan":        "#00BCD4",
    "green":       "#4ADE80",
    "yellow":      "#FACC15",
    "red":         "#EF4444",
    "gray":        "#888888",
    "borderCard":  "#3A3A5A",
    "borderSubtle":"#2A2A4A",
    "borderFocus": "#0078D4",
    # Derived
    "radiusCard":   12,
    "radiusButton":  8,
    "radiusSmall":   6,
    "sidebarWidth": 260,
}
# Exact font from QML Labels
FONT = "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"
# Detail output font (DiagnosticScreen detail dialog)
FONT_MONO = "DejaVu Sans Mono, monospace"

# ═══════════════════════════════════════════════════════════════
# Device definitions
# ═══════════════════════════════════════════════════════════════
DEVICES = {
    # iOS phones (scale factor 3x, compact)
    "ios/phone/6.1": {"w": 375, "h": 812, "os": "ios", "name": "iPhone 6.1\""},
    "ios/phone/6.3": {"w": 402, "h": 874, "os": "ios", "name": "iPhone 6.3\""},
    "ios/phone/6.5": {"w": 414, "h": 896, "os": "ios", "name": "iPhone 6.5\""},
    "ios/phone/6.9": {"w": 440, "h": 956, "os": "ios", "name": "iPhone 6.9\""},
    # iOS tablet (scale factor 2x, wide)
    "ios/tablet/13": {"w": 1032, "h": 1376, "os": "ios", "name": "iPad 13\""},
    # Android phones (compact)
    "android/phone/6.1": {"w": 360, "h": 800, "os": "android", "name": "Android Phone 6.1\""},
    "android/phone/6.3": {"w": 412, "h": 915, "os": "android", "name": "Android Phone 6.3\""},
    "android/phone/6.5": {"w": 430, "h": 932, "os": "android", "name": "Android Phone 6.5\""},
    "android/phone/6.9": {"w": 450, "h": 980, "os": "android", "name": "Android Phone 6.9\""},
    # Android tablet (wide)
    "android/tablet/10": {"w": 800, "h": 1280, "os": "android", "name": "Android Tablet 10\""},
    # Desktop (always wide, multiple sizes)
    "desktop/wide/800":  {"w": 800, "h": 600,  "os": "desktop", "name": "Desktop 800×600"},
    "desktop/wide/1024": {"w": 1024, "h": 768, "os": "desktop", "name": "Desktop 1024×768"},
    "desktop/wide/1280": {"w": 1280, "h": 800, "os": "desktop", "name": "Desktop 1280×800"},
    "desktop/wide/1440": {"w": 1440, "h": 900, "os": "desktop", "name": "Desktop 1440×900"},
}
SCREENS = ["dashboard", "diagnostics", "config", "report", "settings"]


# ═══════════════════════════════════════════════════════════════
# SVG Builder (minimal, fast)
# ═══════════════════════════════════════════════════════════════
class S:
    def __init__(s, w, h): s.W, s.H = w, h; s.p = []; s._d = []
    def r(s, x, y, w, h, rx=0, f=None, st=None, sw=1, o=None, cl=None):
        sx = f'{x:.1f}'; sy = f'{y:.1f}'; sw_ = f'{w:.1f}'; sh_ = f'{h:.1f}'
        a = [f'x="{sx}" y="{sy}" width="{sw_}" height="{sh_}"']
        if rx: a.append(f'rx="{rx}"')
        if f: a.append(f'fill="{f}"')
        if st: a.append(f'stroke="{st}" stroke-width="{sw}"')
        if o is not None: a.append(f'opacity="{o}"')
        if cl: a.append(f'class="{cl}"')
        s.p.append(f'<rect {" ".join(a)}/>')
    def t(s, x, y, txt, sz=12, clr=C["textPrimary"], a="start", w="normal"):
        t = html.escape(str(txt))
        s.p.append(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{FONT}" font-size="{sz}" fill="{clr}" text-anchor="{a}" font-weight="{w}">{t}</text>')
    def tm(s, x, y, txt, sz=10, clr=C["textSecondary"]):
        """Mono detail output text."""
        t = html.escape(str(txt))
        s.p.append(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{FONT_MONO}" font-size="{sz}" fill="{clr}">{t}</text>')
    def c(s, cx, cy, r, f=None, st=None, sw=1):
        a = [f'cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}"']
        if f: a.append(f'fill="{f}"')
        if st: a.append(f'stroke="{st}" stroke-width="{sw}"')
        s.p.append(f'<circle {" ".join(a)}/>')
    def ln(s, x1, y1, x2, y2, st=C["borderCard"], sw=1):
        s.p.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{st}" stroke-width="{sw}"/>')
    def g(s): s.p.append('<g>'); return _G(s)
    def cb(s, x, y, checked, sz=16):
        """CheckBox from QML."""
        s.r(x, y, sz, sz, rx=3, f=C["blue"] if checked else "none", st=C["blue"] if checked else "#5A5A7A", sw=1.5)
        if checked: s.t(x+sz/2, y+sz/2+3, "✓", sz=10, clr="#FFFFFF", a="middle")
    def swt(s, x, y, on):
        """Switch from ConfigScreen QML."""
        if on: s.r(x, y, 40, 22, rx=11, f=C["blue"]); s.c(x+29, y+11, 9, f="#FFFFFF")
        else:  s.r(x, y, 40, 22, rx=11, f="#5A5A7A"); s.c(x+11, y+11, 9, f="#FFFFFF")
    def badge(s, x, y, n, clr):
        """DashboardBadge: implicitWidth:22 implicitHeight:16 radius:4."""
        if n <= 0: return 0
        s.r(x, y, 22, 16, rx=4, f=_a(clr, 0.15))
        s.t(x+11, y+11, str(n), sz=10, clr=clr, a="middle", w="bold")
        return 26
    def pbar(s, x, y, w, h, pct, clr):
        s.r(x, y, w, h, rx=h/2, f=C["borderSubtle"])
        if pct > 0: s.r(x, y, w*pct, h, rx=h/2, f=clr)
    def to_xml(s):
        lns = [f'<?xml version="1.0" encoding="UTF-8"?>',
               f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {s.W} {s.H}" width="{s.W}" height="{s.H}">']
        if s._d: lns.append('<defs>'); lns.extend(s._d); lns.append('</defs>')
        lns.extend(s.p); lns.append('</svg>')
        return '\n'.join(lns)

class _G:
    def __init__(s, svg): s.s = svg
    def __enter__(s): return s.s
    def __exit__(s, *a): s.s.p.append('</g>')

def _a(hex_color, alpha):
    c = hex_color.lstrip('#')
    r, g, b = int(c[0:2],16), int(c[2:4],16), int(c[4:6],16)
    return f"rgba({r},{g},{b},{alpha})"


# ═══════════════════════════════════════════════════════════════
# Shared Nav Bar — AppContent.qml exact
# ═══════════════════════════════════════════════════════════════
def nav_bar(s, W, compact, active_tab):
    """AppContent.qml ColumnLayout > Rectangle nav bar.

    Compact (mobile): implicitHeight=32, tabs 44px wide, icon only, right-aligned
    Desktop: implicitHeight=36, tabs 100px wide, icon+text, centered, close button
    """
    bar_h = 32 if compact else 36
    s.r(0, 0, W, bar_h, f=C["appBar"])

    TABS = [
        ("dashboard",  "⊞", "Dashboard"),
        ("diagnostic", "⚡", "Diag"),
        ("config",     "⚙", "Config"),
        ("report",     "📄", "Report"),
        ("settings",   "⚙", "Settings"),
    ]
    tab_w = 44 if compact else 100
    # Center on desktop, right-align on mobile
    total_w = len(TABS) * tab_w
    if compact:
        start_x = W - total_w - 4
    else:
        start_x = (W - total_w) // 2

    for i, (screen, icon, label) in enumerate(TABS):
        x = start_x + i * tab_w
        active = (i == active_tab)
        row_cy = bar_h / 2

        # Active tab background: Qt.alpha(Theme.cyan, 0.12), radius 6
        if active:
            s.r(x, (bar_h-32)/2, tab_w, 32, rx=6, f=_a(C["cyan"], 0.12))

        if compact:
            # Icon only, centered (AppIcon name=modelData.icon size=14)
            clr = C["cyan"] if active else _a(C["textPrimary"], 0.55)
            s.t(x + tab_w/2, row_cy + 3, icon, sz=14, clr=clr, a="middle")
        else:
            # Icon (size=12) + Label (pixelSize=10, weight=active?DemiBold:Normal)
            clr_icon = C["cyan"] if active else _a(C["textPrimary"], 0.55)
            clr_lbl  = C["cyan"] if active else _a(C["textPrimary"], 0.7)
            lbl_w = "bold" if active else "normal"
            s.t(x + 14, row_cy + 3, icon, sz=12, clr=clr_icon, a="middle")
            s.t(x + 28, row_cy + 3, label, sz=10, clr=clr_lbl, w=lbl_w)

    # Close button — desktop only (Rectangle width:28 height:28 radius:6)
    if not compact:
        cx = W - 22
        s.r(cx - 14, 4, 28, 28, rx=6, f="transparent", st="#5A5A7A")
        s.t(cx, bar_h/2 + 3, "✕", sz=12, clr=_a(C["textPrimary"], 0.7), a="middle")

    return bar_h


# ═══════════════════════════════════════════════════════════════
# Diagnostic SidebarContent — exact from DiagnosticScreen.qml
# ═══════════════════════════════════════════════════════════════
def diag_sidebar(s, top, H, sidebar_w, compact, target="192.168.1.1"):
    """Full SidebarContent component from DiagnosticScreen.qml."""
    sw = sidebar_w
    sx = 12  # Layout.leftMargin: 12

    s.r(0, top, sw, H - top, f=C["bgSidebar"])

    cy = top

    # ── Header: Rectangle implicitHeight:48 border bottom #3A3A5A ──
    s.r(0, cy, sw, 48, f="transparent", st=C["borderCard"])
    s.t(16, cy + 30, "NetDiagnostics", sz=16, clr=C["textPrimary"], w="bold")
    cy += 48 + 12  # Item { Layout.preferredHeight: 12 }

    # ── TargetInputPanel ──
    # "Target" label (pixelSize:11 weight:DemiBold color:textSecondary)
    s.t(sx, cy, "Target", sz=11, clr=C["textSecondary"], w="bold")
    cy += 6 + 6  # Item Layout.preferredHeight:6 + label height ~6px

    # Input field: Rectangle implicitHeight:40 radius:8 bgInput bg, border
    inp_w = sw - 24
    s.r(sx, cy, inp_w, 40, rx=8, f=C["bgInput"], st=C["borderCard"])
    # Placeholder text (color: Qt.alpha(textSecondary, 0.4), pixelSize:12)
    s.t(sx + 10, cy + 25, "Enter URL, IP address, or hostname...", sz=12,
        clr=_a(C["textSecondary"], 0.4))
    cy += 40 + 8  # field + spacing

    # ── Run/Stop buttons ──
    # Run: Layout.fillWidth, implicitHeight:38, radius:8, color:accentBlue
    btn_w = inp_w - 48  # 48 for stop button
    s.r(sx, cy, btn_w, 38, rx=8, f=C["blue"])
    s.t(sx + btn_w/2, cy + 25, "▶ Run Diagnostics", sz=12, clr="#FFFFFF", a="middle", w="bold")
    # Stop: width:48 (actually Layout.preferredWidth:80, but QML uses visible binding)
    s.r(sx + btn_w + 6, cy, 42, 38, rx=8, f="transparent", st=_a(C["red"], 0.5))
    s.t(sx + btn_w + 6 + 21, cy + 25, "■", sz=11, clr=C["red"], a="middle")
    cy += 38 + 8  # buttons + spacing

    if compact:
        return cy  # Compact: only shows target + run button

    # ── Diagnosis Group label (pixelSize:11, weight:DemiBold, color:textSecondary) ──
    s.t(sx, cy, "Diagnosis Group", sz=11, clr=C["textSecondary"], w="bold")
    cy += 6 + 11  # Item Layout.preferredHeight:6 + text

    # ── G1-G5 checkboxes (implicitHeight:32, radius:6) ──
    groups = [
        ("G1 System & Adapters", True, True),
        ("G2 Connectivity & Security", True, True),
        ("G3 Internet & DNS", True, True),
        ("G4 Remote Host", False, True),
        ("G5 Website / URL", False, False),
    ]
    for gname, checked, enabled in groups:
        bg = _a(C["blue"], 0.12) if checked else "transparent"
        s.r(sx, cy, inp_w, 32, rx=6, f=bg)
        s.cb(sx + 8, cy + 8, checked)
        clr = C["textPrimary"] if checked else (C["textSecondary"] if enabled else _a(C["textSecondary"], 0.5))
        s.t(sx + 32, cy + 20, gname, sz=12, clr=clr)
        cy += 32 + 2  # spacing: 2

    cy += 6  # Item Layout.preferredHeight: 8 → 6 remaining

    # ── PortScanConfig (radius:8, bg: Qt.alpha(bgCard,0.5), border: Qt.alpha(blue,0.3)) ──
    ps_h = 106
    s.r(sx, cy, inp_w, ps_h, rx=8, f=_a(C["bgCard"], 0.5), st=_a(C["blue"], 0.3))
    # Header: AppIcon portscan size:14 + Label "Port Scan" pixelSize:11 weight:DemiBold color:blue
    s.t(sx + 10, cy + 16, "◉ Port Scan", sz=11, clr=C["blue"], w="bold")
    # Common ports checkbox
    s.cb(sx + 10, cy + 26, True)
    s.t(sx + 30, cy + 36, "Scan Common Ports", sz=11, clr=C["textPrimary"])
    # Range label
    s.t(sx + 10, cy + 56, "Range:", sz=11, clr=C["textSecondary"])
    # From/To fields (TextField implicitHeight:28, radius:4)
    s.r(sx + 10, cy + 62, 96, 28, rx=4, f="transparent", st=C["borderCard"])
    s.t(sx + 18, cy + 80, "From", sz=11, clr=C["textSecondary"])
    s.t(sx + 114, cy + 80, "–", sz=13, clr=C["textSecondary"])
    s.r(sx + 126, cy + 62, 96, 28, rx=4, f="transparent", st=C["borderCard"])
    s.t(sx + 134, cy + 80, "To", sz=11, clr=C["textSecondary"])
    cy += ps_h + 8

    # ── Divider (height:20, line center:y=10) ──
    s.r(sx, cy, inp_w, 20, f="transparent")
    s.ln(sx, cy + 10, sw - sx, cy + 10, C["borderCard"])
    cy += 20

    # ── TargetAnalysisPanel (radius:8, Qt.alpha(bgCard,0.5), border Qt.alpha(blue,0.3)) ──
    ta_h = 74
    s.r(sx, cy, inp_w, ta_h, rx=8, f=_a(C["bgCard"], 0.5), st=_a(C["blue"], 0.3))
    s.t(sx + 10, cy + 16, "Target Analysis", sz=11, clr=C["blue"], w="bold")
    s.t(sx + 10, cy + 32, "Type    : IPv4 Address", sz=10, clr=C["textSecondary"])
    s.t(sx + 10, cy + 48, "Host    : 192.168.1.1", sz=10, clr=C["textPrimary"])
    s.t(sx + 10, cy + 64, "RFC1918 Private", sz=10, clr=C["textSecondary"])
    cy += ta_h + 8

    # ── SummaryCards at bottom ──
    # Border top + SummaryCards component
    s.r(0, cy, sw, H - cy, f="transparent", st=C["borderCard"])
    sum_cy = cy + 8
    s.t(sx, sum_cy + 8, "Summary", sz=11, clr=C["textSecondary"], w="bold")
    s.t(sw - sx, sum_cy + 8, "Total: 38", sz=10, clr=C["textSecondary"], a="end")
    sum_cy += 16
    for lbl, clr, n in [("Pass", C["green"], 32), ("Warning", C["yellow"], 4),
                         ("Fail", C["red"], 1), ("Skipped", C["gray"], 1)]:
        s.r(sx, sum_cy, inp_w, 26, rx=6, f=_a(clr, 0.06), st=_a(clr, 0.2))
        s.t(sx+8, sum_cy+16, lbl, sz=10, clr=_a(C["textSecondary"], 0.8))
        s.t(sw-16, sum_cy+18, f"{n:2d}", sz=14, clr=clr, a="end", w="bold")
        sum_cy += 29

    return H


# ═══════════════════════════════════════════════════════════════
# Screen 1: DashboardScreen.qml
# ═══════════════════════════════════════════════════════════════
def scr_dashboard(s, W, H, compact, os_name):
    top = nav_bar(s, W, compact, active_tab=0)

    # AppBar: implicitHeight:52, color:#1A1A2E, border bottom #3A3A5A
    app_h = 52
    s.r(0, top, W, app_h, f=C["appBar"])
    s.ln(0, top+app_h-1, W, top+app_h-1, C["borderCard"])
    # AppIcon dashboard size:20 color:cyan + Label pixelSize:15 weight:DemiBold
    s.t(42, top+app_h/2+4, "Dashboard", sz=15, clr=C["textPrimary"], w="bold")

    # Reset button (visible: hasData): implicitWidth:60 implicitHeight:32 radius:6
    s.r(W-76, top+10, 60, 32, rx=6, f="transparent", st="#5A5A7A")
    s.t(W-46, top+app_h/2+4, "Reset", sz=12, clr=C["textSecondary"], a="middle")

    # Content: Flickable > ColumnLayout width:parent.width-48 x:24
    pad = 24  # QML: width: parent.width-48, x: 24
    cw = W - pad*2
    cy = top + app_h + 24  # Item Layout.preferredHeight:24

    # ── Run Info Header Card: bgCard, radius:12, border #2A2A4A ──
    card_h = 80
    s.r(pad, cy, cw, card_h, rx=12, f=C["bgCard"], st=C["borderSubtle"])
    s.t(pad+16, cy+24, "Diagnostic Run Complete", sz=16, clr=C["textPrimary"], w="bold")
    s.t(pad+16, cy+46, "Target: 192.168.1.1", sz=12, clr=C["textSecondary"])
    s.t(pad+16, cy+64, "14:32:18", sz=12, clr=C["textSecondary"])
    cy += card_h + 24

    # ── SummaryCards (compact:true) ──
    s.t(pad, cy, "Summary", sz=11, clr=C["textSecondary"], w="bold")
    s.t(W-pad, cy, "Total: 38", sz=10, clr=C["textSecondary"], a="end")
    cy += 16
    for lbl, clr, n in [("Pass", C["green"], 32), ("Info", C["blue"], 0),
                         ("Warning", C["yellow"], 4), ("Fail", C["red"], 1),
                         ("Skipped", C["gray"], 1)]:
        s.r(pad, cy, cw, 32, rx=6, f=_a(clr, 0.06), st=_a(clr, 0.2))
        s.t(pad+8, cy+19, lbl, sz=10, clr=_a(C["textSecondary"], 0.8))
        s.t(W-pad-8, cy+21, f"{n:3d}", sz=16, clr=clr, a="end", w="bold")
        cy += 36  # 32+4 (Layout.bottomMargin:4)

    cy += 16  # Item Layout.preferredHeight: 32 → after summary

    # ── Per-Group Results header (pixelSize:15 weight:DemiBold) ──
    s.t(pad, cy, "Per-Group Results", sz=15, clr=C["textPrimary"], w="bold")
    cy += 12 + 15  # Item Layout.preferredHeight:12 + text

    # ── DashboardGroupRow per group ──
    groups = [
        ("G1: System & Adapters", 8, 8, 0, 0, 0, 0.95, "2.1s"),
        ("G2: Connectivity & Security", 6, 6, 0, 0, 0, 0.85, "1.8s"),
        ("G3: Internet & DNS", 5, 4, 1, 0, 0, 0.65, "3.4s"),
        ("G4: Remote Host", 6, 6, 0, 0, 0, 0.92, "2.2s"),
        ("G5: Website / URL", 13, 8, 3, 1, 1, 0.78, "5.1s"),
    ]
    for gname, total, p, w, f, sk, pct, dur in groups:
        gh = 88
        s.r(pad, cy, cw, gh, rx=10, f=C["bgCard"], st=C["borderSubtle"])

        # RowLayout: accent bar(3x20) + name(13px bold) + badges + duration
        bar_x = pad + 14  # anchors leftMargin:14
        s.r(bar_x, cy+14, 3, 20, rx=2, f=C["blue"])  # color:accentBlue
        s.t(bar_x+11, cy+18, gname, sz=13, clr=C["textPrimary"], w="bold")

        # Badges from right
        bx = W - pad - 14
        for cnt, clr in [(sk, C["gray"]), (f, C["red"]), (w, C["yellow"]), (p, C["green"])]:
            if cnt > 0:
                bx -= 26 + 4
                s.r(bx, cy+12, 22, 16, rx=4, f=_a(clr, 0.15))
                s.t(bx+11, cy+12+11, str(cnt), sz=10, clr=clr, a="middle", w="bold")

        # Duration (pixelSize:11)
        s.t(W-pad-14, cy+32, dur, sz=11, clr=C["textSecondary"], a="end")

        # Progress bar (implicitHeight:4, radius:2)
        pr_y = cy + 44
        pr_w = cw - 28  # leftMargin:14 + rightMargin:14
        pct_clr = C["red"] if f > 0 else (C["yellow"] if w > 0 else C["green"])
        s.r(bar_x, pr_y, pr_w, 4, rx=2, f=C["borderSubtle"])
        s.r(bar_x, pr_y, pr_w*pct, 4, rx=2, f=pct_clr)

        # Tree items (Repeater model: resultsForGroup)
        ti_y = cy + 56
        for ti in range(min(2, total)):
            if ti_y > cy+gh-8: break
            # Vertical connector line
            s.ln(bar_x+8, ti_y-6, bar_x+8, ti_y+6, C["borderSubtle"])
            # Horizontal stub
            s.ln(bar_x+8, ti_y+4, bar_x+18, ti_y+4, C["borderSubtle"])
            ic = C["red"] if ti >= total-f else (C["yellow"] if ti >= total-f-w else C["green"])
            s.c(bar_x+20, ti_y+4, 4, f=ic)
            s.t(bar_x+28, ti_y+7, f"Test {ti+1}", sz=11, clr=C["textSecondary"])
            s.t(W-pad-14, ti_y+7, f"{10+ti*5}ms", sz=10, clr=_a(C["textSecondary"], 0.6), a="end")
            ti_y += 14

        cy += gh + 8  # Layout.bottomMargin: 8

    cy += 12

    # ── Overall Summary (Rectangle bgCard, radius:12, border #2A2A4A) ──
    sum_h = 148
    s.r(pad, cy, cw, sum_h, rx=12, f=C["bgCard"], st=C["borderSubtle"])
    s.t(pad+16, cy+22, "Summary", sz=15, clr=C["textPrimary"], w="bold")
    iy = cy + 54
    for lbl, val, clr in [("Total Diagnostics", "38", C["cyan"]),
                           ("Total Time", "12.4s", C["blue"]),
                           ("Completed", "38", C["green"])]:
        s.t(pad+16, iy, lbl, sz=12, clr=C["textSecondary"])
        s.t(W-pad-16, iy+4, val, sz=16, clr=clr, a="end", w="bold")
        iy += 24
    s.ln(pad+16, iy, W-pad-16, iy, C["borderSubtle"])
    iy += 16
    s.t(pad+16, iy, "Layer Timings", sz=12, clr=C["textSecondary"], w="bold")
    iy += 16
    for gi, gn in enumerate(["G1", "G2", "G3", "G4", "G5"]):
        if iy > cy+sum_h-8: break
        s.r(pad+16, iy, 8, 8, rx=2, f=C["blue"])
        s.t(pad+30, iy+8, gn, sz=12, clr=C["textPrimary"])
        s.t(W-pad-16, iy+8, f"{2.0+gi*0.8:.1f}s", sz=12, clr=C["textSecondary"], a="end")
        iy += 18

    return s


# ═══════════════════════════════════════════════════════════════
# Screen 2: DiagnosticScreen.qml
# ═══════════════════════════════════════════════════════════════
def scr_diagnostics(s, W, H, compact, os_name):
    top = nav_bar(s, W, compact, active_tab=1)
    is_wide = W >= 600  # QML: readonly property bool wide: width >= 600

    if is_wide:
        sidebar_w = 260
        diag_sidebar(s, top, H, sidebar_w, compact=False)
        s.ln(sidebar_w, top, sidebar_w, H, C["borderCard"])
        content_x = sidebar_w + 1
        content_w = W - content_x
    else:
        sidebar_h = int((H - top) * 0.29)
        diag_sidebar(s, top, top+sidebar_h, W, compact=True)
        s.ln(0, top+sidebar_h, W, top+sidebar_h, C["borderCard"])
        content_x = 0
        content_w = W
        top = top + sidebar_h + 1

    # ── Content header (Rectangle implicitHeight:41 color:#1A1A2E) ──
    s.r(content_x, top, content_w, 41, f=C["appBar"])
    # Label pixelSize:15 weight:DemiBold
    s.t(content_x+16, top+26, "Results", sz=15, clr=C["textPrimary"], w="bold")
    s.t(content_x+content_w-16, top+26, "38 / 38", sz=12, clr=C["cyan"], a="end", w="bold")

    cy = top + 45  # 41 header + 4 margin
    cpad = 8

    # ── DiagGroupPanel per visible group ──
    groups_data = [
        ("G1: System & Adapters", 8, 8, 0, 0, 0, True),
        ("G2: Connectivity & Security", 6, 6, 0, 0, 0, True),
        ("G3: Internet & DNS", 5, 4, 1, 0, 0, True),
        ("G4: Remote Host", 6, 6, 0, 0, 0, True),
        ("G5: Website / URL", 13, 8, 3, 1, 1, True),
    ]

    for gi, (gname, total, p, w_, f, sk, expanded) in enumerate(groups_data):
        gh = min(88, max(60, (H - cy - 20) // len(groups_data)))
        if cy + gh >= H - 10: break

        # Card: radius:10, bgCard, border #2A2A4A
        s.r(content_x+cpad, cy, content_w-cpad*2, gh, rx=10, f=C["bgCard"], st=C["borderSubtle"])

        # Header: accent bar 3×24 + Label G{N}: + group name
        hdr = cy + 8
        s.r(content_x+cpad+12, hdr, 3, 22, rx=2, f=C["blue"])
        s.t(content_x+cpad+23, hdr+4, f"G{gi+1}: {gname.split(': ')[1] if ': ' in gname else gname}", sz=13, clr=C["textPrimary"], w="bold")

        # Count (completed/enabled) — pixelSize:11 weight:Medium color:textSecondary
        s.t(content_x+cpad+content_w-cpad*2-28, hdr+14, f"{p}/{total}", sz=11, clr=C["textSecondary"], a="end")

        # Badges (26×18, radius:4)
        bx = content_x+cpad+content_w-cpad*2-56
        for cnt, clr in [(sk, C["gray"]), (f, C["red"]), (w_, C["yellow"]), (p, C["green"])]:
            if cnt > 0:
                bx -= 28
                s.r(bx, hdr, 26, 18, rx=4, f=_a(clr, 0.15))
                s.t(bx+13, hdr+12, str(cnt), sz=10, clr=clr, a="middle", w="bold")

        # Expand arrow (▼/▶, pixelSize:10, color:textSecondary)
        s.t(content_x+cpad+content_w-cpad*2-14, hdr+4, "▼", sz=10, clr=C["textSecondary"], a="end")

        # Progress bar (implicitHeight:4, Layout.topMargin:6)
        pct = p / total if total > 0 else 0
        pr_clr = C["cyan"]  # isRunning? Not in this mock
        s.r(content_x+cpad+12, cy+38, content_w-cpad*2-24, 4, rx=2, f=C["borderSubtle"])
        s.r(content_x+cpad+12, cy+38, (content_w-cpad*2-24)*pct, 4, rx=2, f=C["green"])

        # Expanded body: separator + tree items
        if expanded:
            sep_y = cy + 46
            s.ln(content_x+cpad+12, sep_y, content_x+cpad+content_w-cpad*2-12, sep_y, C["borderSubtle"])
            ti_y = sep_y + 4
            for ti in range(min(2, total)):
                if ti_y > cy+gh-6: break
                # Tree connector
                s.r(content_x+cpad+20, ti_y, 2, 12, f=C["borderSubtle"])
                stat_clr = C["red"] if ti >= total-f else (C["yellow"] if ti >= total-f-w_ else C["green"])
                s.c(content_x+cpad+28, ti_y+6, 4, f=stat_clr)
                s.t(content_x+cpad+36, ti_y+9, f"Test {gi+1}.{ti+1}", sz=12, clr=C["textSecondary"])
                dur = f"{10+ti*5}ms"
                s.r(content_x+cpad+content_w-cpad*2-60, ti_y+2, len(dur)*7+12, 18, rx=4, f=C["borderSubtle"])
                s.t(content_x+cpad+content_w-cpad*2-16, ti_y+9, dur, sz=10, clr=C["textSecondary"], a="end")
                ti_y += 14

        cy += gh + 4  # spacing: 4

    return s


# ═══════════════════════════════════════════════════════════════
# Screen 3: ConfigScreen.qml
# ═══════════════════════════════════════════════════════════════
def scr_config(s, W, H, compact, os_name):
    top = nav_bar(s, W, compact, active_tab=2)

    # AppBar: implicitHeight:84 = 44(title) + 38(tabs) + 2(border)
    # ColumnLayout fills AppBar: first RowLayout(44px), then Rectangle(tabs, 38px)
    app_h = 84
    s.r(0, top, W, app_h, f=C["appBar"])
    s.ln(0, top+app_h-1, W, top+app_h-1, C["borderCard"])

    # Title row (Layout.preferredHeight:44)
    # AppIcon config size:20 color:cyan + Label pixelSize:15 weight:DemiBold
    s.t(42, top+26, "Diagnostic Config", sz=15, clr=C["textPrimary"], w="bold")

    # Tab bar (Layout.preferredHeight:38)
    tab_y = top + 44
    tab_names = ["G1", "G2", "G3", "G4", "G5"]
    tab_w = W // len(tab_names)
    for i, tn in enumerate(tab_names):
        x = i * tab_w
        active = (i == 0)
        # active tab: cyan bottom border 2px
        if active: s.r(x, tab_y+36, tab_w, 2, f=C["cyan"])
        # Label pixelSize:12, weight:active?DemiBold:Normal, color:active?cyan:textSecondary
        s.t(x+tab_w/2, tab_y+22, tn, sz=12,
            clr=C["cyan"] if active else C["textSecondary"],
            a="middle", w="bold" if active else "normal")

    # ── Action bar (Rectangle: Qt.alpha(bgCard,0.5), border #2A2A4A, implicitHeight:60) ──
    ay = top + app_h
    s.r(0, ay, W, 60, f=_a(C["bgCard"], 0.5), st=C["borderSubtle"])
    # Group name (pixelSize:14, weight:DemiBold)
    s.t(16, ay+24, "G1 System & Adapters", sz=14, clr=C["textPrimary"], w="bold")
    s.t(16, ay+42, "8 diagnostics", sz=11, clr=C["textSecondary"])
    # Select All / Deselect All (implicitWidth:110, implicitHeight:32, radius:6)
    sa_x = W - 244
    s.r(sa_x, ay+14, 110, 32, rx=6, f="transparent", st=C["borderCard"])
    s.t(sa_x+55, ay+32, "Select All", sz=11, clr=C["textPrimary"], a="middle")
    s.r(sa_x+118, ay+14, 110, 32, rx=6, f="transparent", st=C["borderCard"])
    s.t(sa_x+118+55, ay+32, "Deselect All", sz=11, clr=C["textPrimary"], a="middle")

    # ── Test list (ListView) ──
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
    for ti in range(min(len(tests), int((H-ly-10)/item_h))):
        iy = ly + ti * item_h
        name, desc, enabled = tests[ti]
        # Separator
        s.ln(16, iy+item_h-1, W-16, iy+item_h-1, C["borderSubtle"])
        # Leading icon (AppIcon badge-check/badge-circle, size:14)
        ic = C["green"] if enabled else C["textMuted"]
        s.c(28, iy+item_h/2, 7, f=ic)
        # Title (pixelSize:13, weight:Medium) + subtitle (pixelSize:11, color:alpha(textSecondary,0.6))
        s.t(50, iy+20, name, sz=13, clr=C["textPrimary"])
        if desc and ti < 5:  # Only show desc for first few to fit
            s.t(50, iy+36, desc[:60], sz=10, clr=_a(C["textSecondary"], 0.6))
        # Switch
        s.swt(W-60, iy+item_h/2-11, enabled)

    return s


# ═══════════════════════════════════════════════════════════════
# Screen 4: ReportScreen.qml
# ═══════════════════════════════════════════════════════════════
def scr_report(s, W, H, compact, os_name):
    top = nav_bar(s, W, compact, active_tab=3)

    # AppBar: implicitHeight:52, color:#1A1A2E, border #3A3A5A
    app_h = 52
    s.r(0, top, W, app_h, f=C["appBar"])
    s.ln(0, top+app_h-1, W, top+app_h-1, C["borderCard"])
    s.t(42, top+app_h/2+4, "Report Preview", sz=15, clr=C["textPrimary"], w="bold")

    # Centered ColumnLayout: isMobile ? 14 : 24 top margin
    cy = top + app_h + (14 if compact else 24)
    cx = W / 2

    # Icon container (isMobile?72:100, radius:24, Qt.alpha(cyan,0.08), border)
    icon_sz = 72 if compact else 100
    s.r(cx-icon_sz/2, cy, icon_sz, icon_sz, rx=24, f=_a(C["cyan"], 0.08), st=_a(C["cyan"], 0.2), sw=1.5)
    # AppIcon report size:36/48 color:alpha(cyan,0.6)
    s.t(cx, cy+icon_sz/2+5, "📄", sz=int(icon_sz*0.45), clr=_a(C["cyan"], 0.6), a="middle")
    cy += icon_sz + (14 if compact else 24)

    # Title (isMobile?19:22, weight:DemiBold)
    title_sz = 19 if compact else 22
    s.t(cx, cy, "Report Preview", sz=title_sz, clr=C["textPrimary"], a="middle", w="bold")
    cy += title_sz + (8 if compact else 12)

    # Subtitle (pixelSize:14, color:alpha(textSecondary,0.6), wrapMode:WordWrap)
    s.t(cx, cy, "Generate a PDF or HTML report from your latest diagnostic run.",
        sz=14, clr=_a(C["textSecondary"], 0.6), a="middle")
    cy += 14 + (16 if compact else 24)

    # Export buttons (ExportButton: implicitHeight:48, radius:10)
    col_w = min(320, W-48)
    btn_x = cx - col_w/2
    for label, accent in [("Preview PDF Summary", C["cyan"]), ("Preview Full HTML Report", C["blue"])]:
        s.r(btn_x, cy, col_w, 48, rx=10, f=_a(accent, 0.10), st=_a(accent, 0.35))
        s.t(btn_x+40, cy+28, label, sz=13, clr=C["textPrimary"])
        cy += 48 + 10

    cy += (18 if compact else 32)

    # Status indicator
    s.r(cx-140, cy, 280, 40, rx=8, f=_a(C["green"], 0.1), st=_a(C["green"], 0.3))
    s.t(cx, cy+24, "38 results available", sz=12, clr=C["green"], a="middle")

    return s


# ═══════════════════════════════════════════════════════════════
# Screen 5: SettingsScreen.qml
# ═══════════════════════════════════════════════════════════════
def scr_settings(s, W, H, compact, os_name):
    top = nav_bar(s, W, compact, active_tab=4)

    # AppBar: implicitHeight:52
    app_h = 52
    s.r(0, top, W, app_h, f=C["appBar"])
    s.ln(0, top+app_h-1, W, top+app_h-1, C["borderCard"])
    s.t(42, top+app_h/2+4, "Settings", sz=15, clr=C["textPrimary"], w="bold")

    pad = 24  # QML: width: parent.width-48, x:24
    cw = W - pad*2
    cy = top + app_h + 24

    # ── Language Section ──
    # SectionHeader: Rectangle(30x30 radius:8 Qt.alpha(cyan,0.1)) + Label(pixelSize:16 weight:DemiBold)
    s.r(pad, cy, 30, 30, rx=8, f=_a(C["cyan"], 0.1))
    s.t(pad+42, cy+20, "Language", sz=16, clr=C["textPrimary"], w="bold")
    cy += 42

    # Card: bgCard, radius:12, border #2A2A4A
    lang_h = 56
    s.r(pad, cy, cw, lang_h, rx=12, f=C["bgCard"], st=C["borderSubtle"])
    # ComboBox: implicitHeight:44, radius:6, bgInput, border #3A3A5A
    s.r(pad+16, cy+6, cw-32, 44, rx=6, f=C["bgInput"], st=C["borderCard"])
    s.t(pad+28, cy+lang_h/2+3, "English", sz=13, clr=C["textPrimary"])
    s.t(W-pad-28, cy+lang_h/2+4, "▾", sz=12, clr=C["textSecondary"], a="end")
    cy += lang_h + 20

    # ── Premium Section (only on mobile: ios/android) ──
    if compact:
        s.r(pad, cy, 30, 30, rx=8, f=_a(C["yellow"], 0.1))
        s.t(pad+42, cy+20, "Premium", sz=16, clr=C["textPrimary"], w="bold")
        cy += 42

        prem_h = 100
        s.r(pad, cy, cw, prem_h, rx=12, f=C["bgCard"], st=C["borderSubtle"])
        s.t(pad+16, cy+24, "Share & email reports require Premium.", sz=12, clr=C["textSecondary"])
        # Restore button: implicitHeight:40, radius:8, Qt.alpha(warnYellow,0.12), border
        s.r(pad+16, cy+44, cw-32, 40, rx=8, f=_a(C["yellow"], 0.12), st=_a(C["yellow"], 0.4))
        s.t(W/2, cy+66, "Restore Purchases", sz=13, clr=C["yellow"], a="middle", w="bold")
        cy += prem_h + 32

    # ── About Section ──
    s.r(pad, cy, 30, 30, rx=8, f=_a(C["blue"], 0.1))
    s.t(pad+42, cy+20, "About", sz=16, clr=C["textPrimary"], w="bold")
    cy += 42

    about_h = min(280, H-cy-10)
    s.r(pad, cy, cw, about_h, rx=12, f=C["bgCard"], st=C["borderSubtle"])

    # App icon (48x48, radius:12, Qt.alpha(blue,0.15))
    s.r(pad+16, cy+16, 48, 48, rx=12, f=_a(C["blue"], 0.15))
    s.t(pad+40, cy+44, "📡", sz=24, clr=C["blue"], a="middle")
    # Version (pixelSize:12, color:textSecondary)
    s.t(pad+78, cy+32, "NetDiagnostics", sz=18, clr=C["textPrimary"], w="bold")
    s.t(pad+78, cy+50, "Version 6.9.0 (Build 20260704)", sz=12, clr=C["textSecondary"])

    # Divider
    s.ln(pad+16, cy+80, W-pad-16, cy+80, C["borderSubtle"])

    # Description + features
    s.t(pad+16, cy+96, "A cross-platform network diagnostic tool", sz=13, clr=C["textSecondary"])
    s.t(pad+16, cy+116, "with real-time testing and detailed reports.", sz=13, clr=C["textSecondary"])
    feats = ["\U0001f4bb Cross-platform (iOS, Android, Desktop)",
             "⚡ Real-time diagnostics",
             "\U0001f4ca Detailed report generation",
             "\U0001f319 Dark theme",
             "\U0001f5a5 Simulator mode (desktop)"]
    fy = cy+144
    for ft in feats:
        if fy > cy+about_h-8: break
        s.t(pad+16, fy, ft, sz=12, clr=_a(C["textSecondary"], 0.8))
        fy += 22

    return s


# ═══════════════════════════════════════════════════════════════
# Generator dispatch
# ═══════════════════════════════════════════════════════════════
GENS = {
    "dashboard":   scr_dashboard,
    "diagnostics": scr_diagnostics,
    "config":      scr_config,
    "report":      scr_report,
    "settings":    scr_settings,
}


def generate_all(output_dir):
    total = len(DEVICES) * len(SCREENS)
    n = 0
    for dev_path, dev in DEVICES.items():
        W, H = dev["w"], dev["h"]
        compact = W < 600  # QML: readonly property bool wide: width >= 600
        os_name = dev["os"]
        for screen in SCREENS:
            svg = S(W, H)
            gen = GENS[screen]
            gen(svg, W, H, compact, os_name)
            out_path = os.path.join(output_dir, dev_path, f"{screen}.svg")
            os.makedirs(os.path.dirname(out_path), exist_ok=True)
            with open(out_path, 'w', encoding='utf-8') as f:
                f.write(svg.to_xml())
            n += 1
            print(f"  [{n}/{total}] {out_path} ({W}×{H}, {'compact' if compact else 'wide'}, {os_name})")
    print(f"\nDone! {n} SVGs generated.")


if __name__ == "__main__":
    import sys
    output = sys.argv[1] if len(sys.argv) > 1 else "resources/doc/figma"
    print(f"Figma v3 — exact QML extraction, multi-OS, multi-size")
    print(f"  {len(DEVICES)} devices × {len(SCREENS)} screens = {len(DEVICES)*len(SCREENS)} SVGs\n")
    generate_all(output)
