# Figma Design Reference

Design specifications for the NetDiagnostics app. Open `design-preview.html` in any browser for an interactive, size-switchable preview of all screens across all platforms.

## Quick Start

```
open doc/figma/design-preview.html
```

The preview shows all 5 screens (Dashboard, Diagnostics, Config, Report, Settings) in phone (<600px) and tablet/desktop (>=600px) layouts, for iOS, Android, and Desktop platforms. Use the toolbar to filter by screen, platform, or size.

## Screen Summary

| Screen | Description | Mobile | Desktop |
|--------|-------------|--------|---------|
| **Dashboard** | Post-run summary with stats, per-group results, target info | Full-width cards | Centered layout with sidebar |
| **Diagnostics** | 5 expandable groups, 38 tests, live progress, detail dialog | Top-down with swipe expand | Sidebar (260px) + content area |
| **Config** | Group toggle, port scan settings, test selection | Single column | Wider layout |
| **Report** | PDF/HTML preview, share flow, Premium IAP dialog | Compact buttons, centered | Larger preview area |
| **Settings** | Language (9), Restore Purchases (mobile), About, debug toggle | Stacked sections | Two-column options |

## Platforms & Sizes

| Platform | Narrow (<600px) | Wide (>=600px) |
|----------|-----------------|-----------------|
| **iOS** | 375 x 812 (iPhone) | 768 x 1024 (iPad) |
| **Android** | 360 x 800 (phone) | 600 x 960 (tablet) |
| **Desktop** | N/A | 800 x 600+ (Windows/macOS/Linux) |

## Design System (from C++ Theme injection)

### Color Palette

| Token | Hex | Usage |
|-------|-----|-------|
| bgDark | #1E1E2E | Page background |
| bgSidebar | #252538 | Sidebar / hover states |
| bgCard | #16213E | Card containers |
| bgInput | #2A2A4A | Input fields, subtle borders |
| AppBar | #1A1A2E | Top bar / navigation header |
| textPrimary | #E0E0E0 | Primary text |
| textSecondary | #A0A0B8 | Secondary / muted text |
| textMuted | #606080 | Dimmed text |
| accentBlue | #0078D4 | Primary action color |
| cyan | #00BCD4 | Running state / active indicators |
| passGreen | #4ADE80 | Test pass badge |
| warnYellow | #FACC15 | Test warning / Premium badge |
| failRed | #EF4444 | Test fail badge |
| skipGray | #888888 | Test skipped badge |
| borderCard | #3A3A5A | Card borders |
| borderSubtle | #2A2A4A | Inner / subtle borders |
| borderFocused | #0078D4 | Focused element border |
| simBg | #2D2D2D | Simulator background |

### Typography

- **Primary**: JetBrains Mono (headings, UI text)
- **Monospace content**: DejaVu Sans Mono (diagnostic output, box-drawing)
- **Fallback**: Noto Sans Mono CJK SC (Chinese), Microsoft YaHei (Windows CJK)
- **Sizes**: 11px (badges/toast), 12px (body/secondary), 13px (button labels), 14-15px (subtitle), 16-19px (headings), 22px (page title)

### Layout

- **Breakpoint**: 600px (wider -> sidebar visible)
- **Sidebar width**: 260px (desktop)
- **Screen padding**: 24px (desktop), 16px (mobile)
- **Card radius**: 8px (standard), 12px (sections), 24px (icon containers)
- **Button radius**: 6-10px
- **AppBar height**: 52px (report), 36px (navigation)

### Current Screens (from QML source)

```
ApplicationWindow
├── Navigation Bar (tabs: Dashboard | Diagnostics | Config | Report | Settings)
│   └── iOS/Android: no desktop nav (single-page with bottom tabs)
└── StackView
    ├── DashboardScreen  — Summary cards + per-group stats + target info
    ├── DiagnosticScreen — 5 expandable groups with live results + detail dialog
    ├── ConfigScreen     — Group toggle + port scan config + test selection
    ├── ReportScreen     — PDF/HTML preview + share flow + Premium IAP dialog
    └── SettingsScreen   — Language (9) + Premium section (Restore on mobile) + About

SimulatorScreen (desktop only):
    └── Device frame wrapper around the same 5 screens
```

## Architecture Notes

- **Theme**: Injected from C++ as `QVariantMap` via `rootContext()->setContextProperty("Theme", theme)`; 20 color constants + 3 radius values
- **Responsive**: `Qt.platform.os === "ios" || Qt.platform.os === "android"` drives mobile/desktop layout branches; screen width >= 600px enables sidebar
- **IAP Flow**: ReportScreen's share button gates behind `appState.isPremium`; two-stage dialog (subscribe prompt -> confirm share); StoreKit (iOS) or direct grant (desktop)
- **Languages**: 9 (EN, FR, DE, RU, IT, ZH_CN, ZH_TW, ES, PT); all strings in `Translations.qml` via `Tr.*` singleton

## Legacy SVGs

The SVG files (`00-design-system.svg` through `05-simulator-desktop.svg`) are legacy reference exports from an earlier design iteration. For the most current design reference, use `design-preview.html`.
