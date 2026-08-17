#!/usr/bin/env python3
"""WCAG contrast audit for the NetDiagnostics M3 palette.

Parses src/Common/View/theme/Palette.js (Dark/Light blocks), computes
WCAG 2.1 contrast ratios for the critical token pairings, and writes a
markdown report to review/theme-contrast-audit.md (gitignored — local only).

Thresholds:
  · text  (WCAG 1.4.3 AA):  4.5:1 normal, 3:1 large
  · icons/graphics/borders (WCAG 1.4.11 AA): 3:1

5WHY (2026-08-17): colors are intentionally NOT fixed by this script — the
user's decision is to keep all values and track deviations in the report.
Known violations are annotated with root-cause notes instead.

Usage:
  python3 scripts/audit-palette-contrast.py            # write report
  python3 scripts/audit-palette-contrast.py --check    # fail if report drifted
"""
# 5WHY (review 2026-08-17): future-annotations 使注解惰性化，兼容 3.8/3.9。
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from palette_common import contrast, parse_palette, read_palette

ROOT = Path(__file__).resolve().parent.parent
REPORT_MD = ROOT / "review/theme-contrast-audit.md"

# (fg, bg, threshold, kind, note) — fg/bg are token names inside Dark/Light.
# kind: text|graphic
PAIRS = [
    # ── text on surfaces ──
    ("onSurface",              "surface",                  4.5, "text",  ""),
    ("onSurfaceVariant",       "surface",                  4.5, "text",  ""),
    ("onSurface",              "surfaceContainerLow",      4.5, "text",  ""),
    ("onSurfaceVariant",       "surfaceContainerLow",      4.5, "text",  ""),
    ("onSurface",              "surfaceContainerHighest",  4.5, "text",  ""),
    ("onSurfaceVariant",       "surfaceContainerHighest",  4.5, "text",  ""),
    ("textMuted",              "surface",                  4.5, "text",  ""),
    ("textMuted",              "surfaceContainerLow",      4.5, "text",  ""),
    ("textPlaceholder",        "surfaceContainerHighest",  4.5, "text",  "placeholder — exempt from WCAG text contrast; tracked for reference"),
    # ── on-accent pairs ──
    ("onPrimary",              "primary",                  4.5, "text",  ""),
    ("onSecondary",            "secondary",                4.5, "text",  ""),
    ("onTertiary",             "tertiary",                 4.5, "text",  ""),
    ("onError",                "error",                    4.5, "text",  ""),
    # ── accent / status colors on surface (icons, graphics) ──
    ("primary",                "surface",                  3.0, "graphic", ""),
    ("secondary",              "surface",                  3.0, "graphic", ""),
    ("tertiary",               "surface",                  3.0, "graphic", ""),
    ("accent",                 "surface",                  3.0, "graphic", ""),
    ("success",                "surface",                  3.0, "graphic", ""),
    ("warning",                "surface",                  3.0, "graphic", ""),
    ("warningStrong",          "surface",                  3.0, "graphic", ""),
    ("fail",                   "surface",                  3.0, "graphic", ""),
    ("error",                  "surface",                  3.0, "graphic", ""),
    ("skip",                   "surface",                  3.0, "graphic", ""),
    ("info",                   "surface",                  3.0, "graphic", ""),
    # ── boundaries ──
    ("outline",                "surface",                  3.0, "graphic", "input borders need 3:1 (WCAG 1.4.11)"),
    ("outlineVariant",         "surface",                  1.0, "decorative", "decorative divider — informational only"),
    ("outlineVariant",         "surfaceContainerLow",      1.0, "decorative", "card border — informational only"),
]


def build_report() -> tuple[str, dict, list, int]:
    """Return (report_text, parsed_palettes, missing_pairs, below_threshold_count).

    5WHY (2026-08-17 review): the deviation prose hardcoded computed ratios
    (≈2.8:1 etc.) as static text — a palette retune made the documented
    ratios silently contradict the live palette while --check still passed
    (the regenerated report byte-matched the generator output).  Ratios are
    now computed at generation time and injected into the prose, so --check
    catches a stale deviation note as a real drift.
    """
    palettes = parse_palette(read_palette())
    lp = palettes["Light"]

    def _dev(fg: str) -> str:
        """f"{ratio:.1f}:1" for a documented light-theme deviation pair.

        5WHY (review 2026-08-17): 曾用 Light.surface（#F8FAFC）计算却声称
        "白底"——数值与历史记录双双漂移。改用纯白 #FFFFFF 与文字一致。
        缺失角色时给出点名报错而非裸 KeyError。
        """
        if fg not in lp:
            raise SystemExit(f"Palette.js Light block is missing role \"{fg}\" — "
                             f"restore the token before generating the audit")
        return f"{contrast(lp[fg], '#FFFFFF'):.1f}:1"

    lines = [
        "# NetDiagnostics 主题对比度审计（WCAG 2.1 AA）",
        "",
        "> 自动生成：`python3 scripts/audit-palette-contrast.py`（勿手改）。",
        "> 阈值：文字 4.5:1（1.4.3 AA）、图标/图形/边界 3:1（1.4.11 AA）。",
        "> 2026-08-17 M3 重命名轮：颜色值保持不变，偏差仅记录不修复（用户决策）。",
        "",
        "## 已知偏差根因（5WHY 摘要）",
        "",
        f"1. **light primary {lp['primary']} / 白底 ≈{_dev('primary')}**：Tailwind sky-500 逐色对译，",
        "   未按图形 3:1 在白底校验；主色在浅色主题仅作图标/选中态（伴随文字或",
        "   加粗形态），非唯一信息载体。",
        f"2. **light tertiary(cyan) {lp['tertiary']} / 白底 ≈{_dev('tertiary')}**：同上 cyan-500 对译。",
        f"3. **light success {lp['success']} / 白底 ≈{_dev('success')}**：2026-07-30 从 #059669 提升后",
        "   仍低于 3:1；状态图标总伴随 badge 文字，可感知性不唯一依赖色相。",
        f"4. **输入边框 outlineVariant light {lp['outlineVariant']} ≈{_dev('outlineVariant')}**：装饰性边框（输入域",
        "   以底色 + 聚焦态 2px primary 边框识别，满足 1.4.11 聚焦可见）。",
        "",
    ]
    missing: list[str] = []
    fails = 0
    for theme in ("Dark", "Light"):
        p = palettes[theme]
        lines.append(f"## {theme} 主题")
        lines.append("")
        lines.append("| 前景 | 背景 | 比率 | 阈值 | 判定 | 类别 |")
        lines.append("|---|---|---|---|---|---|")
        for fg, bg, thr, kind, note in PAIRS:
            if fg not in p or bg not in p:
                # 5WHY (review 2026-08-17): 角色重命名曾让配对被静默跳过——
                # 审计覆盖面缩小且无任何信号。显式记录 + --check 硬失败。
                absent = [t for t in (fg, bg) if t not in p]
                missing.append(f"{theme}: {'/'.join(absent)} missing for pair {fg}/{bg}")
                continue
            ratio = contrast(p[fg], p[bg])
            verdict = "PASS" if ratio >= thr else ("⚠️ LOW" if kind == "text" else "LOW")
            marker = "✅" if ratio >= thr else "⚠️"
            if ratio < thr and kind in ("text", "graphic"):
                fails += 1
            lines.append(f"| {fg} | {bg} | {ratio:.2f}:1 | {thr:g}:1 | {marker} {verdict} | {kind} |")
        lines.append("")
    return "\n".join(lines) + "\n", palettes, missing, fails


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="fail (exit 1) if the committed report is out of date")
    args = ap.parse_args()

    report, palettes, missing, fails = build_report()
    if args.check:
        if missing:
            print("audit pairs reference missing palette roles:")
            for m in missing:
                print("  " + m)
            return 1
        if not REPORT_MD.exists():
            print("contrast audit report missing — run: python3 scripts/audit-palette-contrast.py")
            return 1
        current = REPORT_MD.read_text(encoding="utf-8")
        if current != report:
            print("contrast audit report out of date — run: python3 scripts/audit-palette-contrast.py")
            return 1
        print("contrast audit report up to date ✓")
        return 0

    for m in missing:
        print("WARNING: " + m)
    REPORT_MD.parent.mkdir(parents=True, exist_ok=True)
    REPORT_MD.write_text(report, encoding="utf-8")
    print(f"wrote {REPORT_MD.relative_to(ROOT)} — {fails} below-threshold pairings (see report)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
