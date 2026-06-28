# Figma Design Files

Design mockups for the NetDiagnostic Qt6 QML application. SVGs are ready for import into Figma — drag-and-drop or File → Import.

## File List

| File | Content | Size |
|------|---------|------|
| `00-design-system.svg` | Design system: color palette, typography, spacing, radii | 900×500 |
| `01-diagnostics-desktop-wide.svg` | Diagnostics page — wide screen (≥600px) with sidebar | 1100×750 |
| `02-diagnostics-phone-narrow.svg` | Diagnostics page — phone narrow (<600px) without sidebar | 375×812 |
| `03-dashboard-desktop.svg` | Dashboard overview with per-group results | 1100×700 |
| `04-config-desktop.svg` | Config page (TabBar + SwitchListTile) | 1100×700 |
| `05-simulator-desktop.svg` | Simulator page with device frame | 1100×700 |

## Layout Architecture (from Qt6 QML source)

```
ApplicationWindow (Qt.FramelessWindowHint)
├── AppContent.qml
│   ├── Navigation Bar (36px, #1A1A2E)
│   │   ├── 5 Tabs: Dashboard | Diagnostics | Config | Report | Settings
│   │   │   (100px each, icon + label, 2px spacing, right-aligned)
│   │   └── ✕ Close button (far right)
│   └── StackView (5 screens, switched by tab)
│       ├── DashboardScreen.qml     — AppBar + Run Info + Group Rows + Summary
│       ├── DiagnosticScreen.qml    — Sidebar(260px) + Content(expandable cards)
│       ├── ConfigScreen.qml        — AppBar + TabBar(G1-G5) + SwitchListTile
│       ├── ReportScreen.qml        — Report preview (planned)
│       └── SettingsScreen.qml      — Application settings
│
└── SimulatorScreen.qml (separate ApplicationWindow)
    └── Device frame + StackView with same 5 tabs
```

## Import into Figma

### Method 1: Drag-and-drop (recommended)
1. Open your Figma project
2. Drag the `.svg` file directly onto the Figma canvas
3. Figma auto-converts SVG to editable Figma layers
4. All colors, text, and shapes are preserved

### Method 2: File → Import
1. Figma menu → File → Import
2. Select the `.svg` file
3. Imported as an independent Frame

### Method 3: Design Tokens plugin
1. Install the "Design Tokens" Figma plugin
2. Use `doc/design-tokens.json`
3. Batch-import all color, typography, and spacing tokens

## Design System (from AppTheme.qml)

| Token | Hex | Usage |
|-------|-----|-------|
| bgDark | #1E1E2E | Page background |
| bgSidebar | #252538 | Sidebar |
| bgCard | #16213E | Card container |
| bgInput | #2A2A4A | Input field / borderSubtle |
| AppBar | #1A1A2E | Top bar / header |
| textPrimary | #E0E0E0 | Primary text |
| textSecondary | #A0A0B8 | Secondary / muted text |
| textMuted | #606080 | Dimmed text |
| accentBlue | #0078D4 | Primary accent (buttons, active) |
| accent | #E94560 | Secondary accent |
| cyan | #00BCD4 | Running / active state |
| passGreen | #4ADE80 | Test pass |
| warnYellow | #FACC15 | Test warning |
| failRed | #EF4444 | Test fail |
| skipGray | #888888 | Test skipped |
| infoBlue | #0078D4 | Info badge |
| borderCard | #3A3A5A | Card / outer border |
| borderSubtle | #2A2A4A | Subtle / inner border |
| borderFocused | #0078D4 | Focused border |
| Sim bg | #2D2D2D | Simulator background |

- **Font**: JetBrains Mono (headings) + DejaVu Sans Mono (monospace content, box-drawing glyphs)
- **Icons**: Phosphor Icons (SVG, via AppIcon.qml)
- **Breakpoint**: ≥600px wide / <600px narrow
- **Sidebar**: 260px (wide screens only)
- **Border radius**: 2 (bars), 4 (badges), 6 (buttons), 8 (cards/inputs), 10 (group cards), 12 (theme cards), 24 (report icon), 55 (device frame)
- **Spacing**: Screen padding 24px, item gap 8px, section gap 24/32px
