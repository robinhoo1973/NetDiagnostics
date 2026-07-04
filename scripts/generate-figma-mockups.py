#!/usr/bin/env python3
"""
Generate SVG design mockups for all screens, platforms, and sizes.
Creates simplified wireframe SVGs showing layout architecture.
"""

import os
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
OUT_DIR = PROJECT / "doc" / "figma"

# ── Design tokens ─────────────────────────────────────────────────────
C = {
    "bg":       "#1E1E2E",
    "card":     "#16213E",
    "bar":      "#1A1A2E",
    "input":    "#2A2A4A",
    "t1":       "#E0E0E0",
    "t2":       "#A0A0B8",
    "t3":       "#606080",
    "cyan":     "#00BCD4",
    "green":    "#4ADE80",
    "yellow":   "#FACC15",
    "red":      "#EF4444",
    "blue":     "#0078D4",
    "border":   "#3A3A5A",
    "sidebar":  "#252538",
}

# ── Platform definitions ──────────────────────────────────────────────
PLATFORMS = {
    "ios": {
        "phone":  {"w": 375, "h": 812, "label": "iPhone 6.1\"", "chrome": True},
        "tablet": {"w": 768, "h": 1024, "label": "iPad", "chrome": True},
    },
    "android": {
        "phone":  {"w": 360, "h": 800, "label": "Android Phone", "chrome": True},
        "tablet": {"w": 600, "h": 960, "label": "Android Tablet", "chrome": True},
    },
    "desktop": {
        "wide": {"w": 1024, "h": 720, "label": "Desktop (Windows/macOS/Linux)", "chrome": False},
    },
}

# ── SVG helpers ───────────────────────────────────────────────────────
def svg_elem(tag, attrs=None, text="", close=True):
    a = " ".join(f'{k}="{v}"' for k,v in (attrs or {}).items())
    if close:
        return f'<{tag} {a}>{text}</{tag}>'
    return f'<{tag} {a}/>'

def rect(x,y,w,h,fill,radius=0,stroke=None):
    a = {"x":x,"y":y,"width":w,"height":h,"fill":fill}
    if radius: a["rx"]=str(radius)
    if stroke: a["stroke"]=stroke; a["stroke-width"]="1"
    return svg_elem("rect", a)

def text(x, y, s, size=12, fill=C["t1"], bold=False):
    a = {"x":x,"y":y,"fill":fill,"font-family":"JetBrains Mono,monospace",
         "font-size":str(size)}
    if bold: a["font-weight"]="bold"
    return svg_elem("text", a, s)

def group(children):
    return f'<g>\n{"".join(children)}\n</g>'

# ── Component: Status bar (iOS/Android) ──────────────────────────────
def status_bar(w, platform):
    y = 0; h = 44 if platform == "ios" else 24
    bg = "#000000" if platform == "ios" else C["bar"]
    return group([
        rect(0,y,w,h,bg),
        text(10, h-12 if platform=="ios" else h-6, "9:41" if platform=="ios" else "12:00",
             10, C["t3"]),
        text(w-50, h-12 if platform=="ios" else h-6,
             "●●●●○  Wi-Fi  ████" if platform=="ios" else "Wi-Fi  ████",
             10, C["t3"]),
    ])

# ── Component: Bottom nav bar (mobile) ───────────────────────────────
def bottom_nav(w, y, h, active=0):
    tabs = ["Dashboard","Diag","Config","Report","Settings"]
    tw = w // len(tabs)
    items = []
    for i, t in enumerate(tabs):
        x = i * tw
        fill = C["cyan"] if i == active else "transparent"
        items += [
            rect(x, y, tw, h, fill, radius=0, stroke=C["border"]),
            text(x+tw//2-20, y+h//2+4, t, 9, C["cyan"] if i==active else C["t3"]),
        ]
    return group(items)

# ── Component: Sidebar ───────────────────────────────────────────────
def sidebar(y, th, active=0):
    w = 200; items = ["Dashboard","Diagnostics","Config","Report","Settings"]
    els = [rect(0,y,w,th,C["sidebar"]), rect(0,y,1,th,C["border"])]
    for i,label in enumerate(items):
        iy = y + 52 + i*44
        bg = C["cyan"] if i==active else "transparent"
        tc = C["bg"] if i==active else C["t1"]
        els += [rect(10,iy,w-20,36,bg,6), text(46,iy+24,label,11,tc)]
    return group(els)

# ── Component: Card ──────────────────────────────────────────────────
def card(x, y, w, title, rows, badge=None):
    h = 44 + len(rows)*22 + 16
    els = [rect(x,y,w,h,C["card"],8,C["border"])]
    tx = x+14
    els.append(text(tx, y+24, title, 12, C["cyan"], True))
    if badge:
        els.append(rect(x+w-60, y+10, 48, 18, C["green"]+"26", 4))
        els.append(text(x+w-56, y+24, badge, 10, C["green"]))
    ry = y+44
    for label,val in rows:
        els.append(text(tx, ry+14, label, 10, C["t2"]))
        els.append(text(x+w-50, ry+14, val, 10, C["t1"]))
        ry += 22
    return group(els)

# ── Component: Button ────────────────────────────────────────────────
def button(x, y, w, h, label, accent=C["cyan"], outline=False):
    fill = accent if not outline else "transparent"
    stroke = accent if outline else "transparent"
    tc = C["bg"] if not outline else accent
    return group([
        rect(x,y,w,h,fill,6,stroke),
        text(x+w//2, y+h//2+4, label, 11, tc, True),
    ])

# ── Screen generators ────────────────────────────────────────────────
def gen_dashboard(w, h, platform, has_sidebar):
    y0 = 44 if (platform in ("ios","android")) else 0
    content_x = 200 if has_sidebar else 0
    content_w = w - content_x
    els = []
    if platform in ("ios","android"):
        els.append(rect(0,0,w,44,C["bar"] if platform=="android" else "#000"))
        els.append(text(16, 32, "Dashboard", 13, C["cyan"], True))
        if platform=="ios":
            els.append(text(w-60, 32, "9:41  Wi-Fi", 10, C["t3"]))
        if not has_sidebar:
            els.append(bottom_nav(w, h-50, 50, 0))
    if has_sidebar:
        els.append(sidebar(y0, h-y0))
    cx, cy = content_x+16, y0+16
    cw = content_w-32
    els.append(card(cx, cy, cw, "Diagnostic Run Complete",
        [("Target","192.168.1.1"),("Total Tests","38"),("Total Time","12.4s"),("Completed","38")]))
    cy += 110
    els.append(card(cx, cy, cw, "Summary",
        [("Pass","32"),("Warning","4"),("Fail","1"),("Skipped","1")]))
    cy += 130
    els.append(card(cx, cy, cw, "Per-Group Results",
        [("G1 System & Adapters","8/8"),("G2 Connectivity","6/6"),
         ("G3 Internet & DNS","4/5"),("G4 Remote Host","6/6"),
         ("G5 Website / URL","12/13")]))
    return group(els)

def gen_diagnostics(w, h, platform, has_sidebar):
    y0 = 44 if (platform in ("ios","android")) else 0
    content_x = 200 if has_sidebar else 0
    content_w = w - content_x
    els = []
    if platform in ("ios","android"):
        els.append(rect(0,0,w,44,C["bar"] if platform=="android" else "#000"))
        els.append(text(16, 32, "Diagnostics", 13, C["cyan"], True))
        if not has_sidebar:
            els.append(bottom_nav(w, h-50, 50, 1))
    if has_sidebar:
        els.append(sidebar(y0, h-y0, 1))
    cx, cy = content_x+16, y0+16
    cw = content_w-32
    groups = [
        ("G1: System & Adapters (8)", "Running...", C["cyan"]),
        ("G2: Connectivity & Security (6)", "", C["t2"]),
        ("G3: Internet & DNS (5)", "", C["t2"]),
        ("G4: Remote Host (6)", "", C["t2"]),
        ("G5: Website / URL (13)", "", C["t2"]),
    ]
    for title, status, color in groups:
        els.append(card(cx, cy, cw, title,
            [("Network Adapters","Pass"),("DHCP Status","Pass"),("IP Configuration","Warn")],
            status if status else None))
        cy += 100
    return group(els)

def gen_config(w, h, platform, has_sidebar):
    y0 = 44 if (platform in ("ios","android")) else 0
    content_x = 200 if has_sidebar else 0
    content_w = w - content_x
    els = []
    if platform in ("ios","android"):
        els.append(rect(0,0,w,44,C["bar"] if platform=="android" else "#000"))
        els.append(text(16, 32, "Config", 13, C["cyan"], True))
        if not has_sidebar:
            els.append(bottom_nav(w, h-50, 50, 2))
    if has_sidebar:
        els.append(sidebar(y0, h-y0, 2))
    cx, cy = content_x+16, y0+16
    cw = content_w-32
    els.append(card(cx, cy, cw, "Port Scan",
        [("Scan Common Ports","On"),("Range","1 - 65535")]))
    cy += 80
    els.append(card(cx, cy, cw, "Diagnostic Configuration",
        [("G1: System & Adapters","Select All"),
         ("G2: Connectivity","Select All"),
         ("G3: Internet & DNS","Deselect All"),
         ("G4: Remote Host","Select All"),
         ("G5: Website / URL","Select All")]))
    return group(els)

def gen_report(w, h, platform, has_sidebar):
    y0 = 44 if (platform in ("ios","android")) else 0
    content_x = 200 if has_sidebar else 0
    content_w = w - content_x
    els = []
    if platform in ("ios","android"):
        els.append(rect(0,0,w,44,C["bar"] if platform=="android" else "#000"))
        els.append(text(16, 32, "Report", 13, C["cyan"], True))
        if not has_sidebar:
            els.append(bottom_nav(w, h-50, 50, 3))
    if has_sidebar:
        els.append(sidebar(y0, h-y0, 3))
    cx = content_x + content_w//2
    cy = y0 + 40
    # Centered content
    icon_sz = 72
    els.append(rect(cx - icon_sz//2, cy, icon_sz, icon_sz, C["cyan"]+"14", 24, C["cyan"]+"33"))
    cy += icon_sz + 16
    els.append(text(cx, cy, "Report Preview", 19 if not has_sidebar else 22, C["t1"], True))
    cy += 28
    els.append(text(cx, cy, "Export your diagnostic results", 12, C["t2"]))
    cy += 20
    els.append(text(cx, cy, "as PDF or HTML.", 12, C["t2"]))
    cy += 30
    bw = min(300, content_w-64)
    els.append(button(cx - bw//2, cy, bw, 36, "Preview PDF (summary)", C["cyan"]))
    cy += 46
    els.append(button(cx - bw//2, cy, bw, 36, "Preview HTML (full)", C["blue"]))
    cy += 56
    # Premium dialog (mobile only, no sidebar)
    if not has_sidebar and platform in ("ios","android"):
        dw = min(300, content_w-40)
        dx = cx - dw//2
        els.append(rect(dx,cy,dw,120,C["card"],10,C["border"]))
        els.append(text(cx, cy+20, "Premium Feature".upper(), 12, C["yellow"], True))
        els.append(text(cx, cy+38, "Sharing is a Premium feature.", 10, C["t2"]))
        els.append(text(cx, cy+52, "One-time purchase for all devices.", 10, C["t2"]))
        els.append(button(dx+14, cy+68, dw-28, 34, "Unlock Premium", C["yellow"]))
    # Results badge
    cy += 80
    els.append(rect(cx-120, cy, 240, 34, C["green"]+"14", 8, C["green"]+"33"))
    els.append(text(cx, cy+22, "38 results available", 11, C["green"]))
    return group(els)

def gen_settings(w, h, platform, has_sidebar):
    y0 = 44 if (platform in ("ios","android")) else 0
    content_x = 200 if has_sidebar else 0
    content_w = w - content_x
    els = []
    if platform in ("ios","android"):
        els.append(rect(0,0,w,44,C["bar"] if platform=="android" else "#000"))
        els.append(text(16, 32, "Settings", 13, C["cyan"], True))
        if not has_sidebar:
            els.append(bottom_nav(w, h-50, 50, 4))
    if has_sidebar:
        els.append(sidebar(y0, h-y0, 4))
    cx, cy = content_x+16, y0+16
    cw = content_w-32
    # Language
    els.append(card(cx, cy, cw, "Language",
        [("Interface Language","English  ▼")]))
    cy += 70
    # Premium (mobile)
    if not has_sidebar and platform in ("ios","android"):
        els.append(card(cx, cy, cw, "Premium Feature",
            [("Status","Locked")]))
        cy += 80
        els.append(button(cx+14, cy, cw-28, 36, "Restore Purchases", C["yellow"], True))
        cy += 52
    # About
    els.append(card(cx, cy, cw, "About",
        [("NetDiagnostics PRO","v0.0.1"),
         ("","Cross-platform network"),
         ("","diagnostic toolkit.")]))
    return group(els)

# ── Main ──────────────────────────────────────────────────────────────
SCREENS = {
    "dashboard":    gen_dashboard,
    "diagnostics":  gen_diagnostics,
    "config":       gen_config,
    "report":       gen_report,
    "settings":     gen_settings,
}

def make_svg(w, h, body_el):
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">
<rect width="{w}" height="{h}" fill="{C['bg']}"/>
{body_el}
</svg>'''

def generate():
    total = 0
    for platform, sizes in PLATFORMS.items():
        for size_key, spec in sizes.items():
            w, h = spec["w"], spec["h"]
            has_sidebar = w >= 600
            for screen_name, gen_fn in SCREENS.items():
                # Build paths: ios/phone/dashboard.svg
                out_path = OUT_DIR / platform / size_key
                out_path.mkdir(parents=True, exist_ok=True)
                fpath = out_path / f"{screen_name}.svg"

                body = gen_fn(w, h, platform, has_sidebar)
                svg = make_svg(w, h, body)
                fpath.write_text(svg)
                total += 1
                print(f"  OK  {platform}/{size_key}/{screen_name}.svg  ({w}x{h})")

    print(f"\nGenerated {total} SVG mockups in {OUT_DIR}/")

if __name__ == "__main__":
    generate()
