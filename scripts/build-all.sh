#!/usr/bin/env bash
# =============================================================================
# build-all.sh — Self-contained build system for NetDiagnostic-QT
#
# Checks dependencies, auto-fixes light-weight tools, builds production
# and simulator binaries across multiple platforms. With --fix, downloads
# missing toolchains from GitHub and builds/installs them globally.
#
# Usage:
#   ./scripts/build-all.sh                              # check deps + native build
#   ./scripts/build-all.sh --check-only                 # only check dependencies
#   ./scripts/build-all.sh --fix                        # auto-fix + check deps
#   ./scripts/build-all.sh --target linux-arm64 --sim   # build + simulator
#   ./scripts/build-all.sh --target all --sim --clean   # all platforms + sim
#   ./scripts/build-all.sh --no-check --clean           # skip dep check
#
# Targets:  linux-arm64 linux-x86_64 windows-x86_64 windows-arm64 all native
# Sim styles: android ios linux windows all
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
# Use /tmp for dist to avoid OneDrive/network-fs interference.
# Symlink project dist/ → /tmp/netdiag-dist for convenience.
DIST_DIR="${TMPDIR:-/tmp}/netdiag-dist"
if [[ -d "$PROJECT_DIR/dist" ]] && [[ ! -L "$PROJECT_DIR/dist" ]]; then
    DIST_DIR="$PROJECT_DIR/dist"
fi
TC_DIR="$PROJECT_DIR/scripts/toolchain"
BUILD_BASE="${TMPDIR:-/tmp}/netdiag-build"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'; BOLD='\033[1m'
PASS="${GREEN}✓${NC}"; FAIL="${RED}✗${NC}"; WARN="${YELLOW}⚠${NC}"; SKIP="${CYAN}○${NC}"

MODE="build"
TARGET="native"; SIM_MODE="off"
SKIP_CHECK=false; CLEAN=false; FIX_DEPS=false
INSTALL_PREFIX="${HOME}/.local"
GLOBAL_PREFIX="/usr/local"
JOBS="$(nproc 2>/dev/null || echo 4)"
TMP_SRC="${TMPDIR:-/tmp}/netdiag-deps-src"

log()  { echo -e "${GREEN}[BUILD]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC}  $*"; }
err()  { echo -e "${RED}[ERROR]${NC} $*"; }
info() { echo -e "${CYAN}[INFO]${NC}  $*"; }

show_help() {
    cat << 'EOF'
NetDiagnostic-QT Build System
─────────────────────────────

Usage: ./scripts/build-all.sh [options]

Options:
  --target <t>     Build target (default: native = current host)
                   linux-arm64 | linux-x86_64 | windows-x86_64 | windows-arm64 | all | native
  --sim            Also build simulator variants (4 OS styles per target)
  --sim-only       Build ONLY simulator variant (skip production binary)
  --check-only     Only run dependency check, do not build
  --fix            Auto-install missing tools from GitHub (ninja/cmake/mingw/Qt6)
  --no-check       Skip dependency check entirely
  --clean          Remove previous build artifacts before building
  --help           Show this help message

Examples:
  ./scripts/build-all.sh                             # default: check + native build
  ./scripts/build-all.sh --check-only                # just verify dependencies
  ./scripts/build-all.sh --fix                       # auto-fix ALL missing deps
  ./scripts/build-all.sh --target all --sim           # all platforms + simulators
  ./scripts/build-all.sh --target windows-x86_64      # cross-compile Windows
  ./scripts/build-all.sh --sim-only                   # simulator only
  ./scripts/build-all.sh --no-check --clean           # clean rebuild, skip dep check

Output: dist/netdiag{-sim}-{platform}[.exe]
EOF
}

# ── Parse args ─────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --target)      TARGET="$2"; shift 2 ;;
        --sim)         SIM_MODE="also"; shift ;;
        --sim-only)    SIM_MODE="only"; shift ;;

        --check-only)  MODE="check-only"; shift ;;
        --fix|-f)      FIX_DEPS=true; shift ;;
        --no-check)    SKIP_CHECK=true; shift ;;
        --clean|-c)    CLEAN=true; shift ;;
        --help|-h)     show_help; exit 0 ;;
        *) err "Unknown: $1 (try --help)"; exit 1 ;;
    esac
done

# ── OS/arch detection ──────────────────────────────────────────────────────
case "$(uname -s)" in
    Linux)  HOST_OS="Linux" ;;
    Darwin) HOST_OS="macOS" ;;
    MINGW*|MSYS*) HOST_OS="Windows" ;;
    *)      HOST_OS="$(uname -s)" ;;
esac
HOST_ARCH="$(uname -m)"
[[ "$HOST_ARCH" == "x86_64" || "$HOST_ARCH" == "amd64" ]]  && HOST_ARCH="x86_64"
[[ "$HOST_ARCH" == "aarch64" || "$HOST_ARCH" == "arm64" ]] && HOST_ARCH="arm64"
[[ "$TARGET" == "native" ]] && TARGET="linux-${HOST_ARCH}"

TARGETS=()
case "$TARGET" in
    all) TARGETS=("linux-arm64" "linux-x86_64" "windows-x86_64" "windows-arm64") ;;
    *)   TARGETS=("$TARGET") ;;
esac
mkdir -p "$DIST_DIR"

# ═════════════════════════════════════════════════════════════════════════════
# Helpers
# ═════════════════════════════════════════════════════════════════════════════
ver_ge() {
    local a="${1:-0}" b="${2:-0}"; [[ "$a" == "$b" ]] && return 0
    local IFS=.; set -- $a; local a1=$1 a2=${2:-0} a3=${3:-0}
    set -- $b; local b1=$1 b2=${2:-0} b3=${3:-0}
    (( a1 > b1 )) && return 0; (( a1 < b1 )) && return 1
    (( a2 > b2 )) && return 0; (( a2 < b2 )) && return 1
    (( a3 >= b3 )) && return 0; return 1
}
cmd_version() {
    local v; v="$("$@" --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)" || true
    echo "${v:-unknown}"
}

# ── Dep check output helpers ───────────────────────────────────────────────
_dep_ok=0; _dep_warn=0; _dep_fail=0; _qt6_ok=true
_d_ok()   { _dep_ok=$((_dep_ok+1));     echo -e "  ${PASS} $1"; }
_d_warn() { _dep_warn=$((_dep_warn+1)); echo -e "  ${WARN} $1 — $2"; }
_d_fail() { _dep_fail=$((_dep_fail+1)); echo -e "  ${FAIL} $1 — $2"; }
_d_skip() { echo -e "  ${SKIP} $1"; }
_d_hdr()  { echo -e "\n${BOLD}── $1 ──${NC}"; }

# ═════════════════════════════════════════════════════════════════════════════
# GitHub download + build helpers
# ═════════════════════════════════════════════════════════════════════════════
_gh_dl() {
    local repo="$1" tag="$2" dest="$3"
    mkdir -p "$(dirname "$dest")"; rm -rf "$dest"
    local url="https://github.com/${repo}/archive/refs/tags/${tag}.tar.gz"
    echo -e "  ${CYAN}→ Fetching ${repo} ${tag}...${NC}"
    # Retry curl up to 3 times for transient network failures
    for i in 1 2 3; do
        if curl -fsSL --retry 2 --retry-delay 5 "$url" -o "${dest}.tar.gz" 2>/dev/null; then
            mkdir -p "$dest"; tar xzf "${dest}.tar.gz" -C "$dest" --strip-components=1; rm -f "${dest}.tar.gz"
            return 0
        fi
        [[ $i -lt 3 ]] && sleep 5
    done
    echo -e "  ${CYAN}→ Tarball not found, cloning via git (HTTP/1.1)...${NC}"
    # Force HTTP/1.1 — HTTP/2 often causes "stream not closed cleanly" errors
    # Also retry up to 3 times with increasing delay
    local git_ok=false
    for i in 1 2 3; do
        if git -c http.version=HTTP/1.1 -c http.postBuffer=524288000 \
               clone --depth 1 "https://github.com/${repo}.git" "$dest" 2>&1; then
            cd "$dest"
            if git fetch --depth 1 origin tag "$tag" 2>&1 && git checkout "tags/${tag}" 2>&1; then
                cd "$OLDPWD"; git_ok=true; break
            fi
            cd "$OLDPWD"
        fi
        echo -e "  ${CYAN}→ Clone attempt ${i}/3 failed, retrying in $((i*10))s...${NC}"
        rm -rf "$dest"
        sleep $((i*10))
    done
    if $git_ok; then return 0; fi
    # Last resort: full tag clone
    echo -e "  ${CYAN}→ Trying full tag clone...${NC}"
    git -c http.version=HTTP/1.1 clone --branch "$tag" --depth 1 \
        "https://github.com/${repo}.git" "$dest" 2>&1 && return 0
    echo -e "  ${RED}✗ Failed to download ${repo} ${tag}${NC}"
    return 1
}

# Download a GNU project tarball from ftp.gnu.org
# Usage: _dl_gnu <project> <version> <dest-dir> [extension]
_dl_gnu() {
    local project="$1" version="$2" dest="$3" ext="${4:-tar.xz}"
    mkdir -p "$(dirname "$dest")"; rm -rf "$dest"
    local url="https://ftp.gnu.org/gnu/${project}/${project}-${version}.${ext}"
    # gcc lives in a versioned subdirectory
    [[ "$project" == "gcc" ]] && url="https://ftp.gnu.org/gnu/gcc/gcc-${version}/gcc-${version}.${ext}"
    echo -e "  ${CYAN}→ Fetching ${project} ${version} from GNU...${NC}"
    if curl -fsSL "$url" -o "${dest}.${ext}" 2>/dev/null; then
        mkdir -p "$dest"
        tar xf "${dest}.${ext}" -C "$dest" --strip-components=1
        rm -f "${dest}.${ext}"
        return 0
    fi
    echo -e "  ${RED}✗ Failed to download ${project} ${version} from GNU${NC}"
    return 1
}

github_dl_build() {
    local tool="$1" repo="$2" tag_prefix="$3" want_ver="$4"
    local tag="${tag_prefix}${want_ver}" src_dir="${TMP_SRC}/${tool}-${want_ver}"
    _gh_dl "$repo" "$tag" "$src_dir" || { echo -e "  ${RED}✗ Failed to download ${tool}${NC}"; return 1; }
    cd "$src_dir"
    echo -e "  ${CYAN}→ Building ${tool}...${NC}"
    case "$tool" in
        ninja) cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" >/dev/null 2>&1
               cmake --build build -j"$JOBS" >/dev/null 2>&1; cmake --install build >/dev/null 2>&1 ;;
        cmake) ./bootstrap --prefix="$INSTALL_PREFIX" --parallel="$JOBS" >/dev/null 2>&1
               make -j"$JOBS" >/dev/null 2>&1; make install >/dev/null 2>&1 ;;
    esac
    cd "$PROJECT_DIR"; rm -rf "$src_dir"
    local bin_path="${INSTALL_PREFIX}/bin/${tool}"
    [[ -x "$bin_path" ]] && { echo -e "  ${GREEN}✓ ${tool} $(cmd_version "$bin_path") → ${bin_path}${NC}"; return 0; }
    echo -e "  ${RED}✗ ${tool} installation failed${NC}"; return 1
}

auto_fix_tool() {
    local tool="$1" req_ver="$2" repo="$3" tag_prefix="$4"
    command -v "$tool" &>/dev/null && { local cv; cv=$(cmd_version "$tool"); ver_ge "$cv" "$req_ver" && { _d_ok "$tool ($cv)"; return 0; }; }
    if $FIX_DEPS; then
        echo -e "  ${CYAN}→ Auto-installing ${tool} ${req_ver}...${NC}"
        github_dl_build "$tool" "$repo" "$tag_prefix" "$req_ver" && { export PATH="${INSTALL_PREFIX}/bin:${PATH}"; _d_ok "$tool (installed)"; return 0; }
    fi
    command -v "$tool" &>/dev/null && _d_warn "$tool" "have $(cmd_version "$tool"), need >= $req_ver — retry with --fix" \
        || _d_fail "$tool" "not found — retry with --fix to auto-install"
}

# ═════════════════════════════════════════════════════════════════════════════
# Heavy toolchain installers (GitHub source → compile → /usr/local)
# ═════════════════════════════════════════════════════════════════════════════

# Ensure a directory exists, using sudo if needed. Returns 0 on success.
_ensure_dir() {
    local dir="$1"
    if [[ -d "$dir" ]]; then return 0; fi
    if mkdir -p "$dir" 2>/dev/null; then return 0; fi
    echo -e "  ${CYAN}→ sudo mkdir -p ${dir}${NC}"
    sudo mkdir -p "$dir" 2>/dev/null && return 0
    echo -e "  ${RED}✗ Cannot create ${dir} — try running with sudo${NC}"
    return 1
}
# Run a command, prefixing with sudo if the dest prefix is not writable
_sudo_maybe() {
    local prefix="${1%/}"; shift
    if [[ -w "$prefix" ]] || [[ -w "$(dirname "$prefix")" && ! -d "$prefix" ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

install_mingw64() {
    # Installs a complete mingw-w64 cross-compiler from source.
    # Builds: binutils → mingw-headers → gcc-stage1 → mingw-crt → gcc-stage2
    # Args: $1 = target triplet (default: x86_64-w64-mingw32)
    #       $2 = install prefix  (default: ${GLOBAL_PREFIX}/mingw-w64)
    # Target: x86_64-w64-mingw32 or aarch64-w64-mingw32
    local target="${1:-x86_64-w64-mingw32}" prefix="${2:-${GLOBAL_PREFIX}/mingw-w64}"
    local src="${TMP_SRC}/mingw-toolchain-${target}"

    if [[ -x "${prefix}/bin/${target}-g++" ]]; then
        echo -e "  ${GREEN}✓ mingw-w64 already installed at ${prefix}${NC}"; return 0
    fi

    echo -e "\n${BOLD}── Building mingw-w64 toolchain (source, ~15 min) ──${NC}"
    mkdir -p "$src"; _ensure_dir "$prefix" || return 1
    local log="${src}/build.log"

    # Helper: run a build step, logging output. Shows tail on failure.
    _run_step() {
        local step_name="$1"; shift
        echo -e "  ${CYAN}→ ${step_name}...${NC}"
        echo "==== ${step_name} ====" >> "$log"
        echo "CMD: $*" >> "$log"
        if "$@" >> "$log" 2>&1; then return 0; fi
        echo -e "  ${RED}✗ ${step_name} failed — last 30 lines of ${log}:${NC}"
        tail -30 "$log" | sed 's/^/      /'
        return 1
    }

    # 1. Binutils (from GNU FTP — GitHub mirror bminor/binutils-gdb is gone)
    _dl_gnu "binutils" "2.42" "$src/binutils" || return 1
    mkdir -p "$src/build-binutils"; cd "$src/build-binutils"
    _run_step "binutils configure" "$src/binutils/configure" --target="$target" --prefix="$prefix" --disable-nls --disable-werror || return 1
    _run_step "binutils make" make -j"$JOBS" MAKEINFO=true || return 1
    _run_step "binutils install" _sudo_maybe "$prefix" make install MAKEINFO=true || return 1

    # 2. MinGW headers
    _gh_dl "mingw-w64/mingw-w64" "v12.0.0" "$src/mingw-w64" || return 1
    mkdir -p "$src/build-headers"; cd "$src/build-headers"
    _run_step "mingw headers configure" "$src/mingw-w64/mingw-w64-headers/configure" --host="$target" --prefix="${prefix}/${target}" || return 1
    _run_step "mingw headers install" _sudo_maybe "$prefix" make install || return 1

    # Symlink for gcc build
    _sudo_maybe "$prefix" ln -sf "${prefix}/${target}" "${prefix}/mingw" 2>/dev/null || true

    # 3. GCC (core, stage 1 — C only)
    _dl_gnu "gcc" "14.2.0" "$src/gcc" || return 1
    # Install GMP/MPFR/MPC — try system packages first (faster + more reliable)
    if ! ( dpkg -s libgmp-dev libmpfr-dev libmpc-dev &>/dev/null ); then
        echo -e "  ${CYAN}→ Installing GCC prerequisites (gmp, mpfr, mpc)...${NC}"
        if command -v apt-get &>/dev/null; then
            sudo apt-get update -qq 2>/dev/null || true
            sudo apt-get install -y libgmp-dev libmpfr-dev libmpc-dev 2>/dev/null || true
        fi
        # Fallback: GCC's own download script
        if ! ( dpkg -s libgmp-dev libmpfr-dev libmpc-dev &>/dev/null ); then
            echo -e "  ${CYAN}→ Trying contrib/download_prerequisites...${NC}"
            ( cd "$src/gcc" && ./contrib/download_prerequisites >> "$log" 2>&1 ) || {
                echo -e "  ${RED}✗ Cannot get GMP/MPFR/MPC. Install manually:${NC}"
                echo -e "  ${RED}   sudo apt-get install libgmp-dev libmpfr-dev libmpc-dev${NC}"
                return 1
            }
        fi
    fi
    mkdir -p "$src/build-gcc"; cd "$src/build-gcc"
    _run_step "gcc stage1 configure" "$src/gcc/configure" --target="$target" --prefix="$prefix" \
        --enable-languages=c,c++ --disable-multilib --disable-nls \
        --with-headers="${prefix}/${target}/include" || return 1
    _run_step "gcc stage1 make" make -j"$JOBS" all-gcc || return 1
    _run_step "gcc stage1 install" _sudo_maybe "$prefix" make install-gcc || return 1

    # 4. MinGW CRT + GCC stage 2
    mkdir -p "$src/build-crt"; cd "$src/build-crt"
    _run_step "mingw crt configure" "$src/mingw-w64/mingw-w64-crt/configure" --host="$target" --prefix="${prefix}/${target}" || return 1
    _run_step "mingw crt make" make -j"$JOBS" || return 1
    _run_step "mingw crt install" _sudo_maybe "$prefix" make install || return 1

    cd "$src/build-gcc"
    _run_step "gcc stage2 make" make -j"$JOBS" || return 1
    _run_step "gcc stage2 install" _sudo_maybe "$prefix" make install || return 1

    cd "$PROJECT_DIR"; rm -rf "$src"

    # Verify
    if [[ -x "${prefix}/bin/${target}-g++" ]]; then
        export PATH="${prefix}/bin:${PATH}"
        echo -e "  ${GREEN}✓ mingw-w64 installed to ${prefix}${NC}"
        echo -e "  ${GREEN}  $("${prefix}/bin/${target}-g++" --version | head -1)${NC}"
        return 0
    fi
    echo -e "  ${RED}✗ mingw-w64 build failed${NC}"; return 1
}

# ── aarch64 (ARM64) Windows cross-compiler via LLVM-MinGW ─────────────────
# GCC does NOT support aarch64-w64-mingw32 (only Clang/LLVM does).
# We use the pre-built LLVM-MinGW toolchain from mstorsjo/llvm-mingw.
install_llvm_mingw_arm64() {
    local prefix="${GLOBAL_PREFIX}/llvm-mingw-arm64"
    local toolchain_ver="20260616"  # update when new releases are available

    if [[ -x "${prefix}/bin/aarch64-w64-mingw32-clang++" ]]; then
        echo -e "  ${GREEN}✓ llvm-mingw-arm64 already installed at ${prefix}${NC}"
        export PATH="${prefix}/bin:${PATH}"
        return 0
    fi

    echo -e "\n${BOLD}── Installing LLVM-MinGW for Windows ARM64 (~77 MB) ──${NC}"
    _ensure_dir "$prefix" || return 1

    local tarball="llvm-mingw-${toolchain_ver}-ucrt-ubuntu-22.04-aarch64.tar.xz"
    local url="https://github.com/mstorsjo/llvm-mingw/releases/download/${toolchain_ver}/${tarball}"
    local dest="${TMP_SRC}/${tarball}"

    echo -e "  ${CYAN}→ Downloading ${tarball}...${NC}"
    if ! curl -fsSL "$url" -o "$dest" 2>/dev/null; then
        echo -e "  ${RED}✗ Download failed — try a newer toolchain_ver in the script${NC}"
        return 1
    fi

    echo -e "  ${CYAN}→ Extracting to ${prefix}...${NC}"
    # Extract to /tmp first (avoids permission issues with /usr/local), then sudo mv
    local tmp_extract="${TMP_SRC}/llvm-mingw-extract"
    rm -rf "$tmp_extract"; mkdir -p "$tmp_extract"
    tar xf "$dest" -C "$tmp_extract" --strip-components=1 2>/dev/null || {
        echo -e "  ${RED}✗ Extraction failed${NC}"; rm -f "$dest"; rm -rf "$tmp_extract"; return 1
    }
    rm -f "$dest"
    _sudo_maybe "$prefix" rm -rf "$prefix" 2>/dev/null || true
    _sudo_maybe "$(dirname "$prefix")" mkdir -p "$(dirname "$prefix")" 2>/dev/null || true
    if [[ -w "$(dirname "$prefix")" ]]; then
        mv "$tmp_extract" "$prefix"
    else
        sudo mv "$tmp_extract" "$prefix"
    fi

    if [[ -x "${prefix}/bin/aarch64-w64-mingw32-clang++" ]]; then
        export PATH="${prefix}/bin:${PATH}"
        echo -e "  ${GREEN}✓ llvm-mingw-arm64 installed to ${prefix}${NC}"
        echo -e "  ${GREEN}  $("${prefix}/bin/aarch64-w64-mingw32-clang++" --version | head -1)${NC}"
        return 0
    fi
    echo -e "  ${RED}✗ llvm-mingw-arm64 install failed${NC}"; return 1
}

install_qt6_mingw() {
    # Installs pre-built Qt6 for mingw-w64.
    # NOTE: Qt 6.x dropped official MinGW pre-built packages (MSVC only).
    # We try win64_mingw first, then provide fallback guidance.
    local qt_ver="6.8.2" target="win64_mingw" prefix="${GLOBAL_PREFIX}/Qt6-mingw"

    if [[ -f "${prefix}/${qt_ver}/mingw_64/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
        echo -e "  ${GREEN}✓ Qt6-mingw already installed at ${prefix}${NC}"; return 0
    fi
    # Also check for MSVC-style layout (llvm-mingw compat)
    if [[ -f "${prefix}/${qt_ver}/mingw_64/bin/qmake" ]] || \
       [[ -f "${prefix}/Qt/${qt_ver}/mingw_64/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
        echo -e "  ${GREEN}✓ Qt6-mingw already installed${NC}"; return 0
    fi

    echo -e "\n${BOLD}── Installing Qt6 for MinGW (pre-built) ──${NC}"

    # Install aqtinstall
    local AQT; AQT="$(command -v aqt 2>/dev/null || echo "")"
    if [[ -z "$AQT" ]]; then
        echo -e "  ${CYAN}→ Installing aqtinstall (from GitHub: miurahr/aqtinstall)...${NC}"
        pip3 install aqtinstall >/dev/null 2>&1 || pip install aqtinstall >/dev/null 2>&1 || {
            echo -e "  ${RED}✗ pip install aqtinstall failed${NC}"; return 1
        }
        AQT="$(command -v aqt 2>/dev/null || echo "")"
        [[ -z "$AQT" ]] && { echo -e "  ${RED}✗ aqt not found after pip install${NC}"; return 1; }
    fi

    # Check what architectures are actually available for this Qt version
    echo -e "  ${CYAN}→ Checking available Qt ${qt_ver} targets...${NC}"
    local avail; avail="$("$AQT" list-qt linux desktop --arch "${qt_ver}" 2>/dev/null || echo "")"
    if [[ -z "$avail" ]]; then
        avail="$("$AQT" list-qt linux desktop 2>/dev/null | grep -F "${qt_ver}" || echo "")"
    fi

    if echo "$avail" | grep -qF "win64_mingw"; then
        echo -e "  ${CYAN}→ Found win64_mingw — downloading...${NC}"
        _ensure_dir "$prefix" || return 1
        if [[ -w "$prefix" ]]; then
            "$AQT" install-qt --outputdir "$prefix" linux desktop "${qt_ver}" win64_mingw >/dev/null 2>&1
        else
            sudo env "PATH=$PATH" "$AQT" install-qt --outputdir "$prefix" linux desktop "${qt_ver}" win64_mingw >/dev/null 2>&1
        fi && { echo -e "  ${GREEN}✓ Qt6-mingw installed${NC}"; return 0; }
    fi

    # Qt 6.x no longer ships MinGW binaries. Provide alternatives.
    echo -e "  ${YELLOW}⚠ Qt ${qt_ver} has no win64_mingw target (Qt 6.x dropped MinGW).${NC}"
    echo -e "  ${CYAN}→ Available targets:${NC}"
    echo "$avail" | head -10 | sed 's/^/      /' 2>/dev/null || true
    echo ""
    echo -e "  ${BOLD}To cross-compile Qt6 for Windows on Linux:${NC}"
    echo -e "    1. MSYS2 (recommended): Install Qt6 in Windows MSYS2 environment"
    echo -e "       pacman -S mingw-w64-x86_64-qt6-base"
    echo -e "    2. Build Qt6 from source with mingw-w64:"
    echo -e "       ./configure -xplatform win32-g++ -prefix /usr/local/Qt6-mingw"
    # ── Fallback: build Qt6 from GitHub source (~1-2 hours) ────────────
    echo -e "  ${CYAN}→ Falling back to source build from GitHub...${NC}"
    install_qt6_mingw_source && return 0
    echo -e "  ${RED}✗ Qt6 source build failed${NC}"; return 1
}

install_qt6_mingw_source() {
    # Builds Qt6 (qtbase + qtdeclarative) from GitHub source with mingw-w64.
    # Args: $1 = target triplet (default: x86_64-w64-mingw32)
    #       $2 = Qt install prefix (default: ${GLOBAL_PREFIX}/Qt6-mingw)
    #       $3 = mingw install prefix (default: ${GLOBAL_PREFIX}/mingw-w64)
    # Time: ~1-2 hours  Disk: ~8 GB in /tmp
    local qt_ver="6.8.2"
    local target="${1:-x86_64-w64-mingw32}"
    local prefix="${2:-${GLOBAL_PREFIX}/Qt6-mingw}"
    local mingw_prefix="${3:-${GLOBAL_PREFIX}/mingw-w64}"
    # Derive CMake SYSTEM_PROCESSOR from triplet (first arch component)
    local cmake_arch="${target%%-*}"  # x86_64 or aarch64

    # Find mingw compiler
    local mingw_gxx; mingw_gxx="$(command -v ${target}-g++ 2>/dev/null || echo "")"
    [[ -z "$mingw_gxx" && -x "${mingw_prefix}/bin/${target}-g++" ]] && mingw_gxx="${mingw_prefix}/bin/${target}-g++"
    [[ -z "$mingw_gxx" ]] && { echo -e "  ${RED}✗ ${target}-g++ not found${NC}"; return 1; }
    local mingw_root; mingw_root="$(dirname "$(dirname "$mingw_gxx")")"

    # Determine mingw-w64 sysroot for cross-compilation isolation.
    # Without CMAKE_SYSROOT, CMake finds host packages (GLib, FreeType, etc.)
    # at /usr/include and adds -I/usr/include to compile commands, which poisons
    # the cross-compiler with Linux glibc headers (bits/libc-header-start.h).
    local mingw_sysroot=""
    for p in "${mingw_root}/${target}" "/usr/${target}"; do
        if [[ -d "$p/include" ]]; then
            mingw_sysroot="$p"; break
        fi
    done
    [[ -z "$mingw_sysroot" ]] && mingw_sysroot="${mingw_root}/${target}"

    if [[ -f "${prefix}/${qt_ver}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
        if [[ -f "${prefix}/${qt_ver}/lib/cmake/Qt6Quick/Qt6QuickConfig.cmake" ]]; then
            echo -e "  ${GREEN}✓ Qt6-mingw (base + Quick) already installed${NC}"; return 0
        fi
        # qtbase installed but Quick missing (qtdeclarative failed earlier).
        echo -e "  ${YELLOW}⚠ Qt6 base exists but Quick is missing.${NC}"
        echo -e "  ${YELLOW}   Run: sudo rm -rf ${prefix}/${qt_ver} && ./scripts/build-all.sh --fix${NC}"
    fi

    echo -e "\n${BOLD}── Building Qt6 from source for mingw-w64 (~1-2 hrs) ──${NC}"
    _ensure_dir "$prefix" || return 1

    local src="${TMP_SRC}/qt6-src-${target}"; mkdir -p "$src"

    # Build a CMake toolchain file for cross-compiling Qt6
    # CRITICAL: Qt6 cross-build needs QT_HOST_PATH pointing to host Qt6 install
    local tc_file="${src}/qt6-mingw-toolchain.cmake"
    # QT_HOST_PATH must point to host Qt install root where CMake can
    # find Qt6HostInfo, Qt6Core, etc.
    local qt_host_path="/usr"
    # Find host Qt6 cmake dir for Qt6HostInfo (Debian multiarch)
    local qt_host_cmake=""
    for p in /usr/lib/aarch64-linux-gnu/cmake/Qt6 /usr/lib/x86_64-linux-gnu/cmake/Qt6 \
             "/usr/lib/$(uname -m)-linux-gnu/cmake/Qt6" /usr/lib/cmake/Qt6; do
        [[ -f "$p/Qt6Config.cmake" ]] && { qt_host_cmake="$p"; break; }
    done
    # Parent of Qt6 cmake dir (e.g. /usr/lib/aarch64-linux-gnu/cmake)
    local qt_host_cmake_root; qt_host_cmake_root="$(dirname "$qt_host_cmake")"

    cat > "$tc_file" << TCEOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ${cmake_arch})
# ── Cross-compilation isolation ──────────────────────────────────────
# CMAKE_SYSROOT redirects host paths so -I/usr/include becomes an
# invalid sysroot-relative path; the cross-compiler never sees Linux
# glibc headers (bits/libc-header-start.h).
set(CMAKE_SYSROOT "${mingw_sysroot}")
# Qt install prefix must be in FIND_ROOT_PATH so qtdeclarative can
# find_package(Qt6) the cross-built qtbase.  Sysroot-only would block it.
set(CMAKE_FIND_ROOT_PATH "${mingw_sysroot};${prefix}/${qt_ver}")
# ── Compilers ────────────────────────────────────────────────────────
set(CMAKE_C_COMPILER ${mingw_gxx/g++/gcc})
set(CMAKE_CXX_COMPILER ${mingw_gxx})
set(CMAKE_RC_COMPILER ${mingw_gxx/g++/windres})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(PKG_CONFIG_EXECUTABLE ${mingw_root}/bin/${target}-pkg-config)
# Qt6 requires host Qt tools for cross-build (moc, rcc, etc.)
# IMPORTANT: QT_HOST_PATH is set but we do NOT add /usr to CMAKE_PREFIX_PATH
# because that leaks host headers (-I/usr/include) into cross-compilation.
set(QT_HOST_PATH "${qt_host_path}" CACHE PATH "Host Qt6 install")
set(Qt6HostInfo_DIR "${qt_host_cmake_root}/Qt6HostInfo" CACHE PATH "")
TCEOF

    # 1. Build qtbase (CMake-based, the ONLY way for Qt6 cross-compile)
    echo -e "  ${CYAN}→ [1/2] Building qtbase (core, network, widgets)...${NC}"
    echo -e "  ${CYAN}    Host Qt6 at: ${qt_host_path}${NC}"
    _gh_dl "qt/qtbase" "v${qt_ver}" "$src/qtbase" || return 1
    rm -rf "$src/build-qtbase"; mkdir -p "$src/build-qtbase"; cd "$src/build-qtbase"
    cmake "$src/qtbase" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$tc_file" \
        -DCMAKE_INSTALL_PREFIX="${prefix}/${qt_ver}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
        -DINPUT_opengl=no \
        > "$src/build-qtbase/cmake.log" 2>&1 || {
        echo -e "  ${RED}✗ qtbase CMake configure failed — see $src/build-qtbase/cmake.log${NC}"
        return 1
    }

    echo -e "  ${CYAN}    Compiling qtbase (this takes a while)...${NC}"
    cmake --build . -j"$JOBS" > "$src/build-qtbase/build.log" 2>&1 || {
        echo -e "  ${RED}✗ qtbase build failed — see $src/build-qtbase/build.log${NC}"
        return 1
    }
    _sudo_maybe "$prefix" cmake --install . >/dev/null 2>&1

    # 2. Build qtdeclarative (Quick, QuickControls2)
    echo -e "  ${CYAN}→ [2/2] Building qtdeclarative (Quick, QuickControls2)...${NC}"
    _gh_dl "qt/qtdeclarative" "v${qt_ver}" "$src/qtdeclarative" || return 1
    mkdir -p "$src/build-qtdeclarative"; cd "$src/build-qtdeclarative"
    cmake "$src/qtdeclarative" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$tc_file" \
        -DCMAKE_INSTALL_PREFIX="${prefix}/${qt_ver}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="${prefix}/${qt_ver}" \
        > "$src/build-qtdeclarative/cmake.log" 2>&1 || {
        echo -e "  ${YELLOW}  qtdeclarative CMake failed — Quick/QuickControls2 unavailable${NC}"
        cd "$PROJECT_DIR"; rm -rf "$src"
        [[ -f "${prefix}/${qt_ver}/lib/cmake/Qt6/Qt6Config.cmake" ]] && { echo -e "  ${GREEN}✓ qtbase installed (Quick unavailable)${NC}"; return 0; }
        return 1
    }
    cmake --build . -j"$JOBS" > "$src/build-qtdeclarative/build.log" 2>&1 || true
    _sudo_maybe "$prefix" cmake --install . >/dev/null 2>&1 || true

    cd "$PROJECT_DIR"; rm -rf "$src"

    if [[ -f "${prefix}/${qt_ver}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
        echo -e "  ${GREEN}✓ Qt6-mingw ${qt_ver} installed to ${prefix}${NC}"
        return 0
    fi
    echo -e "  ${RED}✗ Qt6 source build failed${NC}"; return 1
}

# ── aarch64 Windows Qt6 source build (requires LLVM-MinGW, not GCC) ─────
# NOTE: This is not auto-invoked by --fix. Building Qt6 for Windows ARM64
# from source requires LLVM-MinGW and significant Qt6 configure adjustments.
# Pre-built Qt6 for Windows ARM64 is available via MSYS2 clangarm64 / vcpkg.
install_qt6_mingw_arm64_source() {
    install_qt6_mingw_source \
        "aarch64-w64-mingw32" \
        "${GLOBAL_PREFIX}/Qt6-mingw-arm64" \
        "${GLOBAL_PREFIX}/llvm-mingw-arm64"
}

install_crossbuild_amd64() {
    # Installs x86_64-linux-gnu cross-compiler.
    # Step 1: try apt-get (fast, recommended)
    # Step 2: build binutils + gcc from GitHub source → ${GLOBAL_PREFIX}/x86_64-linux-gnu
    local target="x86_64-linux-gnu" prefix="${GLOBAL_PREFIX}/${target}"

    if [[ -x "${prefix}/bin/${target}-g++" ]]; then
        echo -e "  ${GREEN}✓ ${target} already installed${NC}"; export PATH="${prefix}/bin:${PATH}"; return 0
    fi

    # Step 1: apt-get (fast path)
    if command -v apt-get &>/dev/null; then
        echo -e "  ${CYAN}→ Trying apt-get install crossbuild-essential-amd64...${NC}"
        if sudo apt-get update -qq 2>/dev/null && sudo apt-get install -y crossbuild-essential-amd64 2>/dev/null; then
            if command -v x86_64-linux-gnu-g++ &>/dev/null; then
                echo -e "  ${GREEN}✓ crossbuild-essential-amd64 installed via apt${NC}"; return 0
            fi
        fi
        echo -e "  ${YELLOW}  apt-get failed — falling back to source build${NC}"
    fi

    # Step 2: Build from GitHub source (~25 min)
    echo -e "\n${BOLD}── Building ${target} toolchain (source, ~25 min) ──${NC}"
    local src="${TMP_SRC}/crossbuild-amd64"; mkdir -p "$src"; _ensure_dir "$prefix" || return 1

    # 2a. Binutils (from GNU FTP — GitHub mirror bminor/binutils-gdb is gone)
    echo -e "  ${CYAN}→ [1/3] Building binutils...${NC}"
    _dl_gnu "binutils" "2.42" "$src/binutils" || return 1
    mkdir -p "$src/build-binutils"; cd "$src/build-binutils"
    "$src/binutils/configure" --target="$target" --prefix="$prefix" --disable-nls --disable-werror >/dev/null 2>&1
    make -j"$JOBS" MAKEINFO=true >/dev/null 2>&1 && _sudo_maybe "$prefix" make install MAKEINFO=true >/dev/null 2>&1 || { echo -e "  ${RED}✗ binutils failed${NC}"; return 1; }

    # 2b. Linux kernel headers (from GitHub — minimal set)
    echo -e "  ${CYAN}→ [2/3] Installing Linux headers...${NC}"
    local hdr_dir="${prefix}/${target}/include"
    _sudo_maybe "$prefix" mkdir -p "$hdr_dir"
    _gh_dl "torvalds/linux" "v6.6" "$src/linux" && {
        make -C "$src/linux" headers_install ARCH=x86_64 INSTALL_HDR_PATH="$hdr_dir" >/dev/null 2>&1
    } || {
        # Fallback: copy system headers
        cp -r /usr/include/linux "$hdr_dir/" 2>/dev/null || true
        cp -r /usr/include/asm-generic "$hdr_dir/" 2>/dev/null || true
    }

    # 2c. GCC (C + C++)
    echo -e "  ${CYAN}→ [3/3] Building gcc...${NC}"
    _dl_gnu "gcc" "14.2.0" "$src/gcc" || return 1
    ( cd "$src/gcc" && ./contrib/download_prerequisites >/dev/null 2>&1 ) || true
    mkdir -p "$src/build-gcc"; cd "$src/build-gcc"
    "$src/gcc/configure" --target="$target" --prefix="$prefix" \
        --enable-languages=c,c++ --disable-multilib --disable-nls \
        --with-headers="$hdr_dir" --without-headers >/dev/null 2>&1
    make -j"$JOBS" all-gcc >/dev/null 2>&1 && _sudo_maybe "$prefix" make install-gcc >/dev/null 2>&1 || {
        echo -e "  ${RED}✗ gcc build failed${NC}"; return 1
    }

    cd "$PROJECT_DIR"; rm -rf "$src"
    if [[ -x "${prefix}/bin/${target}-g++" ]]; then
        export PATH="${prefix}/bin:${PATH}"
        echo -e "  ${GREEN}✓ ${target} installed to ${prefix}${NC}"; return 0
    fi
    echo -e "  ${RED}✗ ${target} build failed${NC}"; return 1
}

# ═════════════════════════════════════════════════════════════════════════════
# Dependency check
# ═════════════════════════════════════════════════════════════════════════════
run_dep_check() {
    local check_all="${1:-false}"
    _dep_ok=0; _dep_warn=0; _dep_fail=0; _qt6_ok=true

    echo ""
    echo "============================================="
    echo " NetDiagnostic-QT  Dependency Check"
    echo "============================================="

    _d_hdr "Build Tools"
    auto_fix_tool "ninja" "1.11" "ninja-build/ninja" "v"
    auto_fix_tool "cmake"  "3.22" "Kitware/CMake"        "v"

    if command -v g++ &>/dev/null; then
        _d_ok "g++ ($(g++ -dumpversion 2>/dev/null || echo installed))"
    elif command -v clang++ &>/dev/null; then
        _d_ok "clang++ ($(clang++ -dumpversion 2>/dev/null || echo installed))"
    else
        _d_fail "C++ compiler" "no g++ or clang++ found"
    fi

    command -v pkg-config &>/dev/null && _d_ok "pkg-config ($(pkg-config --version 2>/dev/null || echo installed))" \
        || _d_warn "pkg-config" "not found — Qt6 detection may fail"

    _d_hdr "Qt6 (native)"
    _qt6_ok=true
    local QT6_MODS=("Core" "Concurrent" "Quick" "QuickControls2" "Widgets" "Network")
    local QT6_CMAKE_DIR=""
    for p in /usr/lib/cmake/Qt6 "/usr/lib/$(uname -m)-linux-gnu/cmake/Qt6" \
             /usr/lib/aarch64-linux-gnu/cmake/Qt6 /opt/Qt6/lib/cmake/Qt6 \
             "$HOME"/Qt/6.*/gcc_64/lib/cmake/Qt6; do
        [[ -d $p ]] && { QT6_CMAKE_DIR="$p"; break; }
    done

    if [[ -n "$QT6_CMAKE_DIR" ]] && [[ -f "$QT6_CMAKE_DIR/Qt6Config.cmake" ]]; then
        _d_ok "Qt6 cmake config ($QT6_CMAKE_DIR)"
        local qt6_cmake_parent; qt6_cmake_parent="$(dirname "$QT6_CMAKE_DIR")"
        for mod in "${QT6_MODS[@]}"; do
            if [[ -f "$QT6_CMAKE_DIR/Qt6${mod}Config.cmake" ]] || \
               [[ -f "$QT6_CMAKE_DIR/Qt6${mod}/Qt6${mod}Config.cmake" ]] || \
               [[ -f "$qt6_cmake_parent/Qt6${mod}/Qt6${mod}Config.cmake" ]]; then
                _d_ok "  Qt6::$mod"
            else
                _d_fail "  Qt6::$mod" "config missing"; _qt6_ok=false
            fi
        done
    else
        for mod in "${QT6_MODS[@]}"; do
            pkg-config --exists "Qt6${mod}" 2>/dev/null && _d_ok "  Qt6${mod}" \
                || { _d_fail "  Qt6${mod}" "not found"; _qt6_ok=false; }
        done
    fi
    $_qt6_ok && echo -e "  ${GREEN}→ Target 'linux-${HOST_ARCH}' is buildable${NC}" \
        || echo -e "  ${RED}→ Qt6 incomplete${NC}"

    _d_hdr "Project Resources"
    [[ -f "$PROJECT_DIR/resources/fonts/JetBrainsMono-Regular.ttf" ]] && \
    [[ -f "$PROJECT_DIR/resources/fonts/JetBrainsMono-Bold.ttf" ]] \
        && _d_ok "JetBrains Mono fonts (bundled)" \
        || _d_fail "JetBrains Mono fonts" "missing"
    [[ -f "$PROJECT_DIR/resources/resources.qrc" ]] && _d_ok "QRC resource file" \
        || _d_fail "QRC resource file" "missing"

    local SYS_PKGS=()

    if $check_all; then
        _d_hdr "Cross-compilation Toolchains"
        # Linux x86_64
        local CROSS_GXX="${GLOBAL_PREFIX}/x86_64-linux-gnu/bin/x86_64-linux-gnu-g++"
        local have_cross=false
        command -v x86_64-linux-gnu-g++ &>/dev/null && have_cross=true
        [[ -x "$CROSS_GXX" ]] && { have_cross=true; export PATH="${GLOBAL_PREFIX}/x86_64-linux-gnu/bin:${PATH}"; }

        if $have_cross; then
            _d_ok "x86_64-linux-gnu-g++ ($($(command -v x86_64-linux-gnu-g++) -dumpversion 2>/dev/null || echo installed))"
            local qt64_ok=false qt64_dir=""
            for p in /usr/lib/x86_64-linux-gnu/cmake/Qt6 "/usr/lib/$(uname -m)-linux-gnu/cmake/Qt6"; do
                [[ -d $p ]] && { qt64_ok=true; qt64_dir="$p"; break; }
            done
            $qt64_ok && _d_ok "  Qt6 x86_64 cross ($qt64_dir)" \
                || { _d_warn "  Qt6 x86_64 cross" "not found"; }
            # Check curl dev for cross-compilation (needed by G5WebsiteUrl / NetworkProbe)
            if [[ -f /usr/include/x86_64-linux-gnu/curl/curl.h ]] || dpkg -s libcurl4-openssl-dev:amd64 &>/dev/null; then
                _d_ok "  libcurl-dev:amd64"
            elif $FIX_DEPS && command -v apt-get &>/dev/null; then
                echo -e "  ${CYAN}→ Installing libcurl4-openssl-dev:amd64...${NC}"
                sudo apt-get update -qq 2>/dev/null || true
                sudo apt-get install -y libcurl4-openssl-dev:amd64 2>/dev/null \
                    && _d_ok "  libcurl-dev:amd64 (installed)" \
                    || _d_warn "  libcurl-dev:amd64" "install failed — build may fail"
            else
                _d_warn "  libcurl-dev:amd64" "not found — retry with --fix or: sudo apt install libcurl4-openssl-dev:amd64"
            fi
        else
            if $FIX_DEPS; then
                echo -e "  ${CYAN}→ Auto-installing crossbuild-essential-amd64...${NC}"
                install_crossbuild_amd64 && {
                    export PATH="${GLOBAL_PREFIX}/x86_64-linux-gnu/bin:${PATH}"
                    _d_ok "crossbuild-essential-amd64 (installed from source)"
                } || _d_fail "crossbuild-amd64" "build from source failed"
            else
                _d_skip "linux-x86_64 target (no cross-compiler) — retry with --fix"
            fi
        fi

        # Windows x86_64 — check for mingw + try --fix auto-install
        local MINGW_GXX="${GLOBAL_PREFIX}/mingw-w64/bin/x86_64-w64-mingw32-g++"
        local have_mingw=false
        command -v x86_64-w64-mingw32-g++ &>/dev/null && have_mingw=true
        [[ -x "$MINGW_GXX" ]] && { have_mingw=true; export PATH="${GLOBAL_PREFIX}/mingw-w64/bin:${PATH}"; }

        if $have_mingw; then
            _d_ok "x86_64-w64-mingw32-g++ ($($(command -v x86_64-w64-mingw32-g++) -dumpversion 2>/dev/null || echo installed))"
            local mq_found=false mingw_qt_dir=""
            for p in /usr/x86_64-w64-mingw32/lib/cmake/Qt6 /usr/lib/x86_64-w64-mingw32/cmake/Qt6 \
                     "${GLOBAL_PREFIX}/Qt6-mingw/"*"/mingw_64/lib/cmake/Qt6"; do
                [[ -d $p ]] && { mq_found=true; mingw_qt_dir="$p"; break; }
            done
            if $mq_found; then
                # Also verify Quick component (qtdeclarative) is present
                if [[ -f "${mingw_qt_dir}/Qt6Quick/Qt6QuickConfig.cmake" ]]; then
                    _d_ok "  Qt6 mingw-w64 cross ($mingw_qt_dir)"
                else
                    _d_warn "  Qt6 mingw-w64" "base found but Quick missing — run: sudo rm -rf ${GLOBAL_PREFIX}/Qt6-mingw && retry --fix"
                fi
            elif $FIX_DEPS; then
                echo -e "  ${CYAN}→ Installing Qt6 for mingw-w64...${NC}"
                install_qt6_mingw && _d_ok "  Qt6 mingw-w64 (installed)" \
                    || _d_fail "  Qt6 mingw-w64" "install failed — build cannot proceed"
            else
                _d_warn "  Qt6 mingw-w64" "not found — retry with --fix"
            fi
        else
            if $FIX_DEPS; then
                echo -e "  ${CYAN}→ Auto-installing mingw-w64 from source...${NC}"
                if install_mingw64; then
                    export PATH="${GLOBAL_PREFIX}/mingw-w64/bin:${PATH}"
                    _d_ok "mingw-w64 (installed from source)"
                    echo -e "  ${CYAN}→ Installing Qt6 for mingw-w64...${NC}"
                    install_qt6_mingw && _d_ok "  Qt6 mingw-w64 (installed)" \
                        || _d_fail "  Qt6 mingw-w64" "install failed"
                else
                    _d_fail "mingw-w64" "build from source failed"
                fi
            else
                _d_skip "windows-x86_64 target (no mingw) — retry with --fix"
            fi
        fi

        # Windows ARM64 — LLVM-MinGW (Clang-based, GCC doesn't support this target)
        local LLVM_GXX="${GLOBAL_PREFIX}/llvm-mingw-arm64/bin/aarch64-w64-mingw32-clang++"
        local have_aarch64=false
        command -v aarch64-w64-mingw32-clang++ &>/dev/null && have_aarch64=true
        [[ -x "$LLVM_GXX" ]] && { have_aarch64=true; export PATH="${GLOBAL_PREFIX}/llvm-mingw-arm64/bin:${PATH}"; }

        if $have_aarch64; then
            _d_ok "aarch64-w64-mingw32-clang++ ($($(command -v aarch64-w64-mingw32-clang++) --version 2>/dev/null | head -1 || echo installed))"
            # Qt6 for Windows ARM64 is not built from source here.
            # Pre-built packages: MSYS2 clangarm64, or vcpkg arm64-windows.
            local aq_found=false aq_dir=""
            for p in "${GLOBAL_PREFIX}/Qt6-mingw-arm64/"*"/lib/cmake/Qt6" \
                     /usr/aarch64-w64-mingw32/lib/cmake/Qt6; do
                [[ -d $p ]] && { aq_found=true; aq_dir="$p"; break; }
            done
            $aq_found && _d_ok "  Qt6 aarch64 mingw ($aq_dir)" \
                || _d_warn "  Qt6 aarch64 mingw" "not found — pre-built Qt6 required (MSYS2 clangarm64, vcpkg)"
        else
            if $FIX_DEPS; then
                echo -e "  ${CYAN}→ Auto-installing LLVM-MinGW for Windows ARM64...${NC}"
                if install_llvm_mingw_arm64; then
                    _d_ok "llvm-mingw-arm64 (installed)"
                else
                    _d_fail "llvm-mingw-arm64" "install failed"
                fi
            else
                _d_skip "windows-arm64 target (no llvm-mingw) — retry with --fix"
            fi
        fi
    fi

    echo ""
    echo "============================================="
    echo -e " Summary: ${GREEN}${_dep_ok} OK${NC}  ${YELLOW}${_dep_warn} warnings${NC}  ${RED}${_dep_fail} failed${NC}"
    echo "============================================="

    if [[ $_dep_fail -gt 0 || $_dep_warn -gt 0 ]]; then
        echo ""
        echo "To fix all missing dependencies (from GitHub source):"
        echo "  ./scripts/build-all.sh --fix --target all"
        echo ""
    elif [[ $_dep_ok -gt 0 ]]; then
        echo -e "  ${GREEN}All dependencies satisfied — ready to build!${NC}"; echo ""
    fi

    [[ $_dep_fail -eq 0 ]]
}

# ═════════════════════════════════════════════════════════════════════════════
# Build functions
# ═════════════════════════════════════════════════════════════════════════════

build_one() {
    local tag="$1" os="$2" arch="$3" qt_path="$4" toolchain="$5" cxx_flags="$6"
    local build_dir="${BUILD_BASE}/${tag}"; local ext=""; [[ "$os" == "Windows" ]] && ext=".exe"
    log "Building: $tag"
    local cmake_args=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF)
    [[ "$toolchain" != "native" ]] && cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="$toolchain")
    [[ -n "$qt_path" ]] && cmake_args+=(-DCMAKE_PREFIX_PATH="$qt_path")
    rm -rf "$build_dir"; mkdir -p "$build_dir"
    info "  Configuring..."
    ( [[ -n "$cxx_flags" ]] && export CMAKE_CXX_FLAGS="$cxx_flags"
      cmake "${cmake_args[@]}" -B "$build_dir" -S "$PROJECT_DIR" > "$build_dir/cmake.log" 2>&1
    ) || { err "  CMake failed — $build_dir/cmake.log"; return 1; }
    info "  Compiling..."
    cmake --build "$build_dir" --target net_diagnostics -j"$JOBS" > "$build_dir/build.log" 2>&1 \
        || { err "  Build failed — $build_dir/build.log"; return 1; }
    cp "$build_dir/net_diagnostics${ext}" "$DIST_DIR/netdiag-${tag}${ext}" || return 1
    log "  → dist/netdiag-${tag}${ext}"
}

build_sim() {
    local tag="$1" os="$2" arch="$3" qt_path="$4" toolchain="$5" cxx_flags="${6:-}"
    local build_dir="${BUILD_BASE}/${tag}-sim"; local ext=""; [[ "$os" == "Windows" ]] && ext=".exe"
    log "Building simulator: $tag"
    local cmake_args=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_SIMULATOR=ON)
    [[ "$toolchain" != "native" ]] && cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="$toolchain")
    [[ -n "$qt_path" ]] && cmake_args+=(-DCMAKE_PREFIX_PATH="$qt_path")
    rm -rf "$build_dir"; mkdir -p "$build_dir"
    info "  Configuring..."
    ( [[ -n "$cxx_flags" ]] && export CMAKE_CXX_FLAGS="$cxx_flags"
      cmake "${cmake_args[@]}" -B "$build_dir" -S "$PROJECT_DIR" > "$build_dir/cmake.log" 2>&1
    ) || { err "  CMake failed — $build_dir/cmake.log"; return 1; }
    info "  Compiling..."
    cmake --build "$build_dir" --target net_diagnostics_sim -j"$JOBS" > "$build_dir/build.log" 2>&1 || {
        warn "  Build failed — $build_dir/build.log"; return 1
    }
    local out_name="netdiag-sim-${tag}"
    cp "$build_dir/net_diagnostics_sim${ext}" "$DIST_DIR/${out_name}${ext}" || true
    log "  → dist/${out_name}${ext}"
}

# ── Platform builds ────────────────────────────────────────────────────────
_maybe_mingw_qt() {
    for p in "${GLOBAL_PREFIX}/Qt6-mingw/"*"/mingw_64/lib/cmake/Qt6" \
             "${GLOBAL_PREFIX}/Qt6-mingw/"*"/lib/cmake/Qt6" \
             /usr/x86_64-w64-mingw32/lib/cmake/Qt6 /usr/lib/x86_64-w64-mingw32/cmake/Qt6; do
        [[ -d $p ]] && { echo "$p"; return 0; }
    done; echo ""
}

build_linux_arm64() {
    local tc="$TC_DIR/linux-arm64.cmake"; [[ -f "$tc" ]] || tc="native"
    [[ "$SIM_MODE" != "only" ]] && build_one "linux-arm64" "Linux" "arm64" "" "$tc" ""
    [[ "$SIM_MODE" != "off" ]]  && build_sim "linux-arm64" "Linux" "arm64" "" "$tc" ""
}
build_linux_x86_64() {
    local gxx; gxx="$(command -v x86_64-linux-gnu-g++ 2>/dev/null || echo "")"
    [[ -z "$gxx" && -x "${GLOBAL_PREFIX}/x86_64-linux-gnu/bin/x86_64-linux-gnu-g++" ]] && gxx="${GLOBAL_PREFIX}/x86_64-linux-gnu/bin/x86_64-linux-gnu-g++"
    [[ -z "$gxx" ]] && { warn "x86_64-linux-gnu-g++ missing — skip (try --fix)"; return 0; }
    export PATH="$(dirname "$gxx"):${PATH}"
    local qt_x64=""
    for p in /usr/lib/x86_64-linux-gnu/cmake/Qt6 "/usr/lib/$(uname -m)-linux-gnu/cmake/Qt6"; do
        [[ -d "$p" ]] && { qt_x64="$p"; break; }
    done
    [[ -z "$qt_x64" ]] && { warn "Qt6 x86_64 missing — skip"; return 0; }
    local tc="$TC_DIR/linux-x86_64.cmake"; [[ -f "$tc" ]] || { warn "Toolchain missing: $tc"; return 0; }
    local flgs="-O2 -static-libgcc -static-libstdc++"
    [[ "$SIM_MODE" != "only" ]] && build_one "linux-x86_64" "Linux" "x86_64" "$qt_x64" "$tc" "$flgs"
    [[ "$SIM_MODE" != "off" ]]  && build_sim "linux-x86_64" "Linux" "x86_64" "$qt_x64" "$tc" "$flgs"
}
build_windows_x86_64() {
    local gxx; gxx="$(command -v x86_64-w64-mingw32-g++ 2>/dev/null || echo "")"
    [[ -z "$gxx" && -x "${GLOBAL_PREFIX}/mingw-w64/bin/x86_64-w64-mingw32-g++" ]] && gxx="${GLOBAL_PREFIX}/mingw-w64/bin/x86_64-w64-mingw32-g++"
    [[ -z "$gxx" ]] && { warn "mingw-w64 missing — skip (try --fix)"; return 0; }
    export PATH="$(dirname "$gxx"):${PATH}"
    local mingw_qt; mingw_qt="$(_maybe_mingw_qt)"
    [[ -z "$mingw_qt" ]] && { warn "Qt6 mingw-w64 missing — skip (try --fix)"; return 0; }
    local tc="$TC_DIR/windows-x86_64.cmake"; local flgs="-O2 -static-libgcc -static-libstdc++ -static"
    [[ "$SIM_MODE" != "only" ]] && build_one "windows-x86_64" "Windows" "x86_64" "$mingw_qt" "$tc" "$flgs"
    [[ "$SIM_MODE" != "off" ]]  && build_sim "windows-x86_64" "Windows" "x86_64" "$mingw_qt" "$tc" "$flgs"
}
build_windows_arm64() {
    # Uses LLVM-MinGW (Clang), not GCC — only Clang supports aarch64-w64-mingw32
    local cxx; cxx="$(command -v aarch64-w64-mingw32-clang++ 2>/dev/null || echo "")"
    [[ -z "$cxx" && -x "${GLOBAL_PREFIX}/llvm-mingw-arm64/bin/aarch64-w64-mingw32-clang++" ]] && cxx="${GLOBAL_PREFIX}/llvm-mingw-arm64/bin/aarch64-w64-mingw32-clang++"
    [[ -z "$cxx" ]] && { warn "llvm-mingw-arm64 missing — skip (try --fix)"; return 0; }
    export PATH="$(dirname "$cxx"):${PATH}"
    local mingw_qt=""
    for p in "${GLOBAL_PREFIX}/Qt6-mingw-arm64/"*"/lib/cmake/Qt6" \
             /usr/aarch64-w64-mingw32/lib/cmake/Qt6; do
        [[ -d "$p" ]] && { mingw_qt="$p"; break; }
    done
    [[ -z "$mingw_qt" ]] && { warn "Qt6 aarch64 mingw missing — skip (try --fix)"; return 0; }
    local tc="$TC_DIR/windows-arm64.cmake"; local flgs="-O2"
    [[ "$SIM_MODE" != "only" ]] && build_one "windows-arm64" "Windows" "arm64" "$mingw_qt" "$tc" "$flgs"
    [[ "$SIM_MODE" != "off" ]]  && build_sim "windows-arm64" "Windows" "arm64" "$mingw_qt" "$tc" "$flgs"
}

# ═════════════════════════════════════════════════════════════════════════════
# Main
# ═════════════════════════════════════════════════════════════════════════════

if ! $SKIP_CHECK; then
    check_all_flag=false
    [[ "${#TARGETS[@]}" -gt 1 || "${TARGETS[0]}" != "linux-${HOST_ARCH}" ]] && check_all_flag=true
    $check_all_flag && echo -e "${BOLD}── Full platform check ──${NC}"
    run_dep_check "$check_all_flag" || { err "Dependency check failed — fix issues or use --no-check"; exit 1; }
    [[ "$MODE" == "check-only" ]] && { echo "Check complete."; exit 0; }
fi

if [[ "$CLEAN" == "true" ]]; then
    rm -rf "$BUILD_BASE"
    # Clean old artifacts: only match names with 3+ segments after "sim-"
    # Old: netdiag-sim-linux-arm64-linux (3 hyphens after sim)
    # New: netdiag-sim-linux-arm64        (2 hyphens after sim) — KEEP
    for f in "$DIST_DIR"/netdiag-sim-*-*-* \
             "$DIST_DIR"/net_diagnostics-* "$DIST_DIR"/net_diagnostics; do
        [[ -f "$f" ]] && rm -f "$f"
    done 2>/dev/null || true
fi

echo ""; echo "============================================="
echo " NetDiagnostic Build — Targets: ${TARGETS[*]}"
echo " Simulator: ${SIM_MODE}"
echo "============================================="; echo ""

BUILT=0  FAILED=0
for target in "${TARGETS[@]}"; do
    case "$target" in
        linux-arm64)    build_linux_arm64    && BUILT=$((BUILT+1)) || FAILED=$((FAILED+1)) ;;
        linux-x86_64)   build_linux_x86_64   && BUILT=$((BUILT+1)) || FAILED=$((FAILED+1)) ;;
        windows-x86_64) build_windows_x86_64 && BUILT=$((BUILT+1)) || FAILED=$((FAILED+1)) ;;
        windows-arm64)  build_windows_arm64  && BUILT=$((BUILT+1)) || FAILED=$((FAILED+1)) ;;
        *) err "Unknown target: $target"; FAILED=$((FAILED+1)) ;;
    esac; echo ""
done

echo "============================================="
echo -e " Build Summary: ${GREEN}${BUILT} succeeded${NC}  ${RED}${FAILED} failed${NC}"
echo "============================================="; echo ""
ls -lh "$DIST_DIR"/netdiag* 2>/dev/null || echo "  (no binaries in dist/)"; echo ""
[[ $FAILED -gt 0 ]] && exit 1
exit 0
