#!/usr/bin/env bash
# =============================================================================
# qml-aot-gate.sh — QML AOT 编译闸门单一来源（pre-commit 21b + CI 双闸门）
#
# 5WHY (simplify 2026-09-04): qmlcachegen 调用（--bare -I src/Common/View 标志集、
# 候选路径发现、逐文件循环）曾在 scripts/pre-commit check 21b 与
# ios-startup-smoke/action.yml Gate 1 双份维护——标志集或发现逻辑漂移后，
# 本地 pre-commit 放行而 CI 闸门拦截（或闸门悄悄不再匹配 iOS AOT 行为）。
# 收敛为单一脚本，两个调用方共享同一契约：
#   · 发现 qmlcachegen（env QMLCACHEGEN 可指定，CI 用确定性 aqt 布局）
#   · 并行逐文件编译（qmlcachegen 单文件单次调用；-o 指向临时目录，
#     .qmlc 不落源码旁、随 trap 清理）
#   · 每文件输出 PASS/FAIL 行（FAIL 后附缩进日志），任一失败 exit 1
#
# 用法: qml-aot-gate.sh <repo-relative-qml-file>...
#   exit 0 = 全部通过；exit 1 = 至少一个文件被拒绝；exit 2 = 未找到编译器。
#   stderr 输出 "TOOL <path>" 供调用方做 Qt 版本核验。
# =============================================================================
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
INCLUDE_DIR="$ROOT/src/Common/View"

QMLCACHEGEN="${QMLCACHEGEN:-}"
if [ -z "$QMLCACHEGEN" ]; then
    for candidate in \
        /usr/lib/qt6/libexec/qmlcachegen \
        /usr/libexec/qt6/libexec/qmlcachegen \
        /usr/bin/qmlcachegen \
        /opt/homebrew/bin/qmlcachegen \
        "$(command -v qmlcachegen 2>/dev/null || true)"; do
        if [ -x "$candidate" ]; then QMLCACHEGEN="$candidate"; break; fi
    done
fi
# CI 兜底：确定性 aqt 布局（/Users/runner/qt-desktop/<ver>/clang_64/bin）之外
# 的版本升级期，find 全树兜底一次（dev 机器无此路径，开销为零）。
# 5WHY (2026-09-05): -name qmlcachegen 会命中 qmlcachegen.dSYM 内的 DWARF
# 调试副本（readdir 顺序不定）→ 误选不可执行路径。排除 *.dSYM/*。
if [ -z "$QMLCACHEGEN" ]; then
    QMLCACHEGEN=$(find /Users/runner/qt-desktop -type f -name qmlcachegen \
        -not -path '*.dSYM/*' 2>/dev/null | head -1)
fi
if [ -z "$QMLCACHEGEN" ]; then
    exit 2
fi
echo "TOOL $QMLCACHEGEN" >&2

STATUS_DIR=$(mktemp -d)
trap 'rm -rf "$STATUS_DIR"' EXIT

check_one() {
    local f="$1"
    local log="$STATUS_DIR/$(basename "$f").log"
    local out="$STATUS_DIR/$(basename "$f").qmlc"
    # 5WHY (2026-09-04 实测): 本版本 qmlcachegen 的 -o 是输出 FILE（非目录；
    # 传目录报 "Filename refers to a directory"）。输出指向临时文件，
    # .qmlc 不落源码旁、随 trap 清理。
    if "$QMLCACHEGEN" --bare -I "$INCLUDE_DIR" -o "$out" "$ROOT/$f" >"$log" 2>&1; then
        echo "PASS $f"
    else
        echo "FAIL $f"
        sed 's/^/       /' "$log"
        touch "$STATUS_DIR/FAILED"
    fi
}
export -f check_one
export QMLCACHEGEN INCLUDE_DIR ROOT STATUS_DIR

# 并行编译：qmlcachegen 冷启动 ~100-300ms/文件，90 文件串行 20-45s → 并行 ~3-5s
printf '%s\n' "$@" | xargs -P"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" \
    -I{} bash -c 'check_one "$1"' _ {}

[ ! -e "$STATUS_DIR/FAILED" ]
