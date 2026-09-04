cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED PROJECT_VERSION)
    set(PROJECT_VERSION "0.0.1")
endif()
project(NetDiagnostic VERSION ${PROJECT_VERSION} LANGUAGES CXX)

option(ND_DEBUG "Enable crash diagnostic startup logging" OFF)
option(ND_TESTING "Enable automated headless testing harness" OFF)
option(ND_STATIC_QT "Import static Qt platform plugins" OFF)
option(ND_STRICT_STATIC_WINDOWS "Require a Windows executable with no non-Windows DLL imports" OFF)

if(NOT DEFINED APP_EDITION)
    set(APP_EDITION "")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── CMake modules ──────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(dependencies)      # Qt6 + curl discovery, iOS SDK detection
include(netdiag-target)    # configure_netdiag_target() + platform bundles
# 5WHY (review 2026-08-17): include() 在 1acc2d62 重构中被移除——AppColors.h↔
# Palette.js 构建期校验从未在 CI 运行，镜像漂移类问题可静默入库。
# 5WHY (simplify 2026-09-04): 自愈 + 比较 + 解释器探针收敛为本模块单一
# 所有者（generate-before-verify 序列），模块导出的 _PYTHON3 /
# _ND_GEN_APPCOLORS 供底部 build-time 增量规则复用。
include(VerifyPaletteSync) # AppColors.h ↔ Palette.js configure-time auto-heal + sync check

# ── Source files (rebuilt foundation — contract + execution core) ──────
# UI layer (PageSection/PageDisplay/sections/pages) and per-group adapters
# G2..G5 are added incrementally per review/refactor/*.
set(SOURCES
    src/main.cpp
    src/app/AppState.cpp
    src/Configuration/Model/DiagnosticConfig.cpp
    src/Configuration/Controller/ConfigurationController.cpp
    src/Common/Model/DiagnosticResult.cpp
    src/Common/Model/DiagnosticMeta.cpp
    src/Common/Utils/NarrativeLocalizer.cpp
    src/Common/Services/Logger.cpp
    src/Common/Services/DiagnosticBase.cpp
    src/Common/Services/AdapterRegistry.cpp
    src/Common/Services/DnsResolver.cpp
    src/Common/Services/ProbeDatabase.cpp
    src/Common/Services/IconProvider.cpp
    src/Common/Platform/DeviceCapability.cpp
    src/Common/Platform/PlatformCredentialStore.cpp
    src/Diagnostics/Model/DiagnosticSuite.cpp
    src/Diagnostics/Model/Adapters.cpp
    src/Diagnostics/Model/GCommon.cpp
    src/Diagnostics/Model/G1/Adapters.cpp
    src/Diagnostics/Model/G2/Adapters.cpp
    src/Diagnostics/Model/G3/Adapters.cpp
    src/Diagnostics/Model/G4/Adapters.cpp
    src/Diagnostics/Model/G5/Adapters.cpp
    src/Diagnostics/Model/GeoProbe.cpp
    src/Diagnostics/Model/ProbeScheduler.cpp
    src/Diagnostics/Model/ProbeExecutor.cpp
    src/Diagnostics/Model/ProbeFeedback.cpp
    src/Diagnostics/View/DiagnosticFormatter.cpp
    src/Report/Model/ReportEngine.cpp
    src/Settings/Model/PremiumStore.cpp
)

# ── Windows resource ───────────────────────────────────────────────────
if(WIN32)
    list(APPEND SOURCES resources/netanalysis.rc)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"${CMAKE_SOURCE_DIR}/resources\"")
endif()

# ── 平台专属源（iOS .mm / Android JNI 深探测 + 分享/PDF/StoreKit）──
if(ANDROID)
    list(APPEND SOURCES
        src/Common/Platform/Android/PlatformShare.cpp
        src/Common/Platform/Android/PlatformPdfRenderer.cpp
        src/Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.cpp
    )
endif()
if(IOS)
    list(APPEND SOURCES
        src/Common/Platform/Apple/IOS/PlatformShare.mm
        src/Common/Platform/Apple/IOS/PlatformPdfRenderer.mm
        src/Common/Platform/Apple/IOS/PlatformStore.mm
        src/Common/Platform/NativePdfDocument.cpp
        src/Diagnostics/Model/G1/Platform/IOS/IosNetworkInfo.mm
        src/Diagnostics/Model/G3/Platform/IOS/DnsResolve.mm
        src/Diagnostics/Model/G5/Platform/IOS/HttpDiagnostics.mm
    )
endif()
if(APPLE AND NOT IOS)
    list(APPEND SOURCES
        src/Common/Platform/Apple/macOS/PlatformPdfRenderer.mm
        src/Common/Platform/Apple/macOS/PlatformStore.mm
        src/Common/Platform/Apple/macOS/WifiHelper.mm
        src/Common/Platform/NativePdfDocument.cpp
    )
endif()
# Keychain 凭证存储为 Objective-C++（CF 字典 + Security.h），必须以 .mm
# 编译；PlatformCredentialStore.cpp 在 Apple 平台编译为空 TU。文件按
# per-OS 目录惯例置于 Apple/（iOS 与 macOS 共享同一实现）。
if(APPLE)
    list(APPEND SOURCES
        src/Common/Platform/Apple/PlatformCredentialStore.mm
    )
endif()

# ── QML resources（基础壳 + sections/widgets/pages + 预着色图标）──
set(QML_RESOURCES
    resources/resources_core.qrc
    resources/resources_icons.qrc
)
# 条件预览组件（模块可用才编入；HAS_* 宏由 netdiag-target 定义）
if(TARGET Qt6::WebView)
    list(APPEND QML_RESOURCES resources/resources_webview.qrc)
endif()
if(TARGET Qt6::Pdf AND TARGET Qt6::PdfQuick)
    list(APPEND QML_RESOURCES resources/resources_qtpdf.qrc)
endif()
if(IOS OR APPLE)
    list(APPEND QML_RESOURCES resources/resources_nativepdf.qrc)
endif()

# ── Production target ──────────────────────────────────────────────────
qt_add_executable(net_diagnostics WIN32 MANUAL_FINALIZATION ${SOURCES} ${QML_RESOURCES})
# "Common/...", "Configuration/...", "Diagnostics/..." 风格 include 的根
target_include_directories(net_diagnostics PRIVATE "${CMAKE_SOURCE_DIR}/src")
setup_platform_bundle(net_diagnostics)
configure_netdiag_target(net_diagnostics)

# ND_TESTING：无头 CI 模式（--test 别名 --selftest，AGENTS.md 契约）
if(ND_TESTING)
    target_compile_definitions(net_diagnostics PRIVATE ND_TESTING)
endif()

# ── Copy qtquickcontrols2.conf to build dir (dev runtime fallback) ──────
set(_QMLCONF "${CMAKE_BINARY_DIR}/qtquickcontrols2.conf")
add_custom_command(TARGET net_diagnostics POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/qtquickcontrols2.conf" "${_QMLCONF}"
    COMMENT "Copying qtquickcontrols2.conf to build dir")

# L6 (5WHY): VerifyPaletteSync.cmake 只在 configure 时运行——开发者编辑
# Palette.js 后不重新 cmake，旧 AppColors.h 被使用导致颜色不一致。
# 加 build-time 自定义命令：Palette.js 变更时自动重新生成 AppColors.h。
# 5WHY (2026-09-04 修正复核): 裸 add_custom_command(OUTPUT …) 无任何目标
# 消费其输出——首次构建（无编译 depfile 引用该头）下规则永不执行，
# 仅在增量构建时碰巧经编译器依赖追踪触发。改为 ALL 自定义目标 +
# add_dependencies 强制每次构建先生成、再编译 net_diagnostics；
# DEPENDS 补上 palette_common.py（生成脚本的导入依赖）。
# 5WHY (simplify 2026-09-04): --write-if-changed 内容一致不落盘——不刷新
# mtime 即不触发下游全量重编译；_PYTHON3/_ND_GEN_APPCOLORS 由
# VerifyPaletteSync.cmake 导出（单一所有者），此处不再重复探针。
if(_PYTHON3)
    add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/src/Common/Utils/AppColors.h"
        COMMAND "${_PYTHON3}" "${_ND_GEN_APPCOLORS}" --write-if-changed
        DEPENDS "${CMAKE_SOURCE_DIR}/src/Common/View/theme/Palette.js"
                "${CMAKE_SOURCE_DIR}/scripts/generate-appcolors.py"
                "${CMAKE_SOURCE_DIR}/scripts/palette_common.py"
        COMMENT "Regenerating AppColors.h from Palette.js if changed")
    add_custom_target(regenerate_appcolors ALL
        DEPENDS "${CMAKE_SOURCE_DIR}/src/Common/Utils/AppColors.h")
    add_dependencies(net_diagnostics regenerate_appcolors)
endif()
