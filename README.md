# NetAnalysis

Cross-platform network diagnostic tool. Built with Qt 6 / QML and libcurl.

## Features

### Diagnostic Groups

| Group | Name | Description |
|-------|------|-------------|
| G1 | System & Adapters | Network adapters, IP configuration, WiFi, DHCP, active connections |
| G2 | Connectivity & Security | Routing table, ARP table, proxy settings, TCP settings |
| G3 | Internet & DNS | DNS servers, DNS cache, DNS pollution check, Internet speed test |
| G4 | Remote Host | DNS resolution, ping, traceroute, pathPing, MTU discovery, port scan |
| G5 | Website / URL | HTTP headers, curl verbose, SSL certificate, redirect, compression, timing |

### Key Features

- **Pure C++ diagnostics** — zero shell commands, direct OS API calls (Linux `/proc`, Windows Win32 API)
- **libcurl integration** — full HTTP/HTTPS support with curl-compatible verbose output
- **DNS resolution** — dig-style output with HEADER/QUESTION/ANSWER/AUTHORITY/ADDITIONAL sections
- **Speed test** — Ookla-compatible download/upload bandwidth measurement
- **Port scanner** — concurrent non-blocking socket scan with range merging
- **7-language UI** — English, French, German, Russian, Italian, Simplified/Traditional Chinese
- **Monospace detail output** — DejaVu Sans Mono for aligned diagnostic tables
- **Badge-style status icons** — colored SVG badges for Pass/Warning/Fail/Skip/Info
- **Single-instance lock** — prevents duplicate application instances

## Build

### Quick Start (automated, all platforms)

```bash
# Check dependencies only
./scripts/build-all.sh --check-only

# Native build (auto-detect host platform)
./scripts/build-all.sh

# Auto-fix ALL missing deps (installs cross-compilers, Qt6, ninja, cmake)
./scripts/build-all.sh --fix --target all

# Cross-compile specific target + simulator
./scripts/build-all.sh --target windows-x86_64 --sim

# Clean rebuild, skip dep check
./scripts/build-all.sh --target linux-arm64 --clean --no-check
```

### Build System Features

| Feature | Description |
|---------|-------------|
| `--fix` | Auto-installs missing tools from source (ninja, cmake, mingw-w64, LLVM-MinGW, Qt6) |
| `--target all` | Builds linux-arm64, linux-x86_64, windows-x86_64, windows-arm64 |
| `--sim` / `--sim-only` | Also build simulator variant with device-frame UI |
| `--clean` | Remove previous build artifacts |
| Cross-compilation | mingw-w64 (x86_64), LLVM-MinGW (aarch64), linux-gnu (x86_64) |
| Smart TMPDIR | Auto-detects small tmpfs and uses `~/.cache` for Qt6 source builds |

### Manual Build (single platform)

### Linux (arm64 / x86_64)

```bash
# Install dependencies
sudo apt install qt6-base-dev qt6-quickcontrols2-dev libcurl4-openssl-dev cmake ninja-build

# Build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
ninja -C build net_diagnostic

# Output: build/net_diagnostic
```

### Windows (cross-compile from Linux)

```bash
# x86_64 — mingw-w64 (GCC)
./scripts/build-all.sh --target windows-x86_64 --fix

# ARM64 — LLVM-MinGW (Clang)
./scripts/build-all.sh --target windows-arm64 --fix
```

### Simulator

```bash
cmake -G Ninja -DBUILD_SIMULATOR=ON -B build -S .
ninja -C build net_diagnostic_sim
```

## Supported Platforms

| Platform | Compiler | Status |
|----------|----------|--------|
| Linux (arm64) | GCC | ✅ Full support |
| Linux (x86_64) | GCC cross-compile | ✅ Full support |
| Windows x86_64 | mingw-w64 (GCC) | ✅ Cross-compile via `--fix` |
| Windows ARM64 | LLVM-MinGW (Clang) | ✅ Compiler ready, Qt6 via MSYS2/vcpkg |
| macOS | — | ⚠️ Partial (G1/G2/G3 limited) |
| iOS | — | ⚠️ Compiles, sandbox restrictions |
| Android | — | ✅ Mostly works |

## License

MIT
