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

# ── AppColors.h ↔ Palette.js configure-time auto-regeneration ──────────
# 5WHY (2026-09-04 修正复核): VerifyPaletteSync.cmake 的 FATAL_ERROR 在
# configure 期硬失败——开发者改 Palette.js 后重新 cmake（IDE kit 切换、
# 全新 CI 目录）会死在 build-time 再生成器跑起来之前，自动修复恰好在其
# 目标场景不可达。修正：include(VerifyPaletteSync) 之前先用生成器自带的
# --check 内容探针自愈，FATAL 只留给生成器自身治不好的失配：
#   1. --check 通过 → 不写文件（不触发下游全量重编译）
#   2. 漂移 → 原位再生成 → VerifyPaletteSync 对新文件验证
#   3. 生成器拒绝（新角色未加 ROLES 等）→ FATAL 保留 fail-closed
# 解释器三处静默失效统一拦截（无版本检查 / Windows Store 桩 / 找不到
# 解释器时机制无声消失）——版本探针失败显式告警而非静默降级。
set(_ND_GEN_APPCOLORS "${CMAKE_SOURCE_DIR}/scripts/generate-appcolors.py")
find_program(_PYTHON3 NAMES python3 python)
if(_PYTHON3)
    execute_process(COMMAND "${_PYTHON3}" -c
        "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)"
        RESULT_VARIABLE _ND_PY_OK OUTPUT_QUIET ERROR_QUIET)
    if(NOT _ND_PY_OK EQUAL 0)
        message(WARNING "Python interpreter '${_PYTHON3}' failed the >=3.8 check — "
                        "AppColors.h auto-regeneration disabled")
        unset(_PYTHON3)
        unset(_PYTHON3 CACHE)
    endif()
endif()
if(_PYTHON3)
    execute_process(COMMAND "${_PYTHON3}" "${_ND_GEN_APPCOLORS}" --check
        RESULT_VARIABLE _ND_SYNC_OK
        OUTPUT_VARIABLE _ND_SYNC_OUT ERROR_VARIABLE _ND_SYNC_ERR)
    if(NOT _ND_SYNC_OK EQUAL 0)
        message(STATUS "AppColors.h out of sync with Palette.js — regenerating")
        execute_process(COMMAND "${_PYTHON3}" "${_ND_GEN_APPCOLORS}"
            RESULT_VARIABLE _ND_GEN_OK
            OUTPUT_VARIABLE _ND_GEN_OUT ERROR_VARIABLE _ND_GEN_ERR)
        if(NOT _ND_GEN_OK EQUAL 0)
            message(FATAL_ERROR
                "AppColors.h regeneration failed (Palette.js ↔ ROLES mismatch?):\n"
                "${_ND_GEN_OUT}${_ND_GEN_ERR}")
        endif()
        message(STATUS "Regenerated: ${_ND_GEN_OUT}")
    endif()
else()
    message(WARNING "python3 >= 3.8 not found — AppColors.h auto-regeneration "
                    "disabled (Palette.js edits will not propagate)")
endif()
# 5WHY (review 2026-08-17): include() 在 1acc2d62 重构中被移除——AppColors.h↔
# Palette.js 构建期校验从未在 CI 运行，镜像漂移类问题可静默入库。
include(VerifyPaletteSync) # AppColors.h ↔ Palette.js configure-time sync check

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
# Keychain 凭证存储为 Objective-C++（字典字面量 + Security.h），
# 必须以 .mm 编译；PlatformCredentialStore.cpp 在 Apple 平台编译为空 TU。
if(APPLE)
    list(APPEND SOURCES
        src/Common/Platform/PlatformCredentialStore.mm
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
# 解释器探针与 configure-time 自愈在文件顶部，此处只消费已验证的 _PYTHON3。
if(_PYTHON3)
    add_custom_command(OUTPUT "${CMAKE_SOURCE_DIR}/src/Common/Utils/AppColors.h"
        COMMAND "${_PYTHON3}" "${CMAKE_SOURCE_DIR}/scripts/generate-appcolors.py"
        DEPENDS "${CMAKE_SOURCE_DIR}/src/Common/View/theme/Palette.js"
                "${CMAKE_SOURCE_DIR}/scripts/generate-appcolors.py"
                "${CMAKE_SOURCE_DIR}/scripts/palette_common.py"
        COMMENT "Regenerating AppColors.h from Palette.js")
    add_custom_target(regenerate_appcolors ALL
        DEPENDS "${CMAKE_SOURCE_DIR}/src/Common/Utils/AppColors.h")
    add_dependencies(net_diagnostics regenerate_appcolors)
endif()
