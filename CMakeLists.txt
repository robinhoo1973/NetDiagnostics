cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED PROJECT_VERSION)
    set(PROJECT_VERSION "0.0.1")
endif()
project(NetDiagnostic VERSION ${PROJECT_VERSION} LANGUAGES CXX)

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

# ── Source files ───────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/models/DiagnosticResult.cpp
    src/util/Logger.cpp
    src/util/DnsResolver.cpp
    src/engine/diagnostics/DiagnosticFormatter.cpp
    src/engine/task/DiagnosticTask.cpp
    src/engine/task/TaskFactory.cpp
    src/engine/diagnostics/NetworkProbe.cpp
    src/app/AppState.cpp
    src/app/DiagnosticConfig.cpp
    src/app/ReportEngine.cpp
    src/app/PremiumStore.cpp
    src/engine/diagnostics/GCommon.cpp
    src/engine/diagnostics/G1/G1NetworkAdapters.cpp
    src/engine/diagnostics/G1/G1ActiveConnections.cpp
    src/engine/diagnostics/G1/G1CellularInfo.cpp
    src/engine/diagnostics/G1/G1IpConfiguration.cpp
    src/engine/diagnostics/G1/G1WifiDiagnostics.cpp
    src/engine/diagnostics/G1/G1NicAdvanced.cpp
    src/engine/diagnostics/G1/G1WiredDiagnostics.cpp
    src/engine/diagnostics/G1/G1DhcpStatus.cpp
    src/engine/diagnostics/G2/G2RoutingTable.cpp
    src/engine/diagnostics/G2/G2ArpTable.cpp
    src/engine/diagnostics/G2/G2NetworkProfile.cpp
    src/engine/diagnostics/G2/G2TcpSettings.cpp
    src/engine/diagnostics/G2/G2DefaultGateway.cpp
    src/engine/diagnostics/G2/G2ProxySettings.cpp
    src/engine/diagnostics/G3/G3NetskopeStatus.cpp
    src/engine/diagnostics/G3/G3DnsServers.cpp
    src/engine/diagnostics/G3/G3DnsCache.cpp
    src/engine/diagnostics/G3/G3DnsPollution.cpp
    src/engine/diagnostics/G3/G3InternetSpeedTest.cpp
    src/engine/diagnostics/G4/G4DnsResolution.cpp
    src/engine/diagnostics/G4/G4Ping.cpp
    src/engine/diagnostics/G4/G4Traceroute.cpp
    src/engine/diagnostics/G4/G4PathPing.cpp
    src/engine/diagnostics/G4/G4PingParser.cpp
    src/engine/diagnostics/G4/G4MtuDiscovery.cpp
    src/engine/diagnostics/G5/G5UrlParsing.cpp
    src/engine/diagnostics/G5/G5TcpConnect.cpp
    src/engine/diagnostics/G5/G5ServiceBanner.cpp
    src/engine/diagnostics/G5/G5HttpHeaders.cpp
    src/engine/diagnostics/G5/G5CurlVerbose.cpp
    src/engine/diagnostics/G5/G5SecurityHeaders.cpp
    src/engine/diagnostics/G5/G5SslCertificate.cpp
    src/engine/diagnostics/G5/G5HttpRedirect.cpp
    src/engine/diagnostics/G5/G5HttpCompression.cpp
    src/engine/diagnostics/G5/G5HttpTiming.cpp
    src/engine/diagnostics/G5/G5FtpDiagnostics.cpp
    src/engine/diagnostics/G5/G5SshDiagnostics.cpp
    src/engine/diagnostics/G5/G5EmailDiagnostics.cpp
    src/engine/diagnostics/G5/G5Telnet.cpp
    src/engine/diagnostics/G5/G5Mysql.cpp
    src/engine/diagnostics/G5/G5Postgres.cpp
    src/engine/diagnostics/G5/G5Redis.cpp
    src/engine/diagnostics/G5/G5Mongodb.cpp
    src/engine/diagnostics/G5/G5Ldap.cpp
    src/engine/diagnostics/G5/G5Mqtt.cpp
)

# ── Automated testing harness (ND_TESTING) ──────────────────────────────
if(ND_TESTING)
    list(APPEND SOURCES
        src/testing/TestHarness.cpp
        src/testing/TestScenarios.cpp
    )
endif()

# ── Platform-specific sources ──────────────────────────────────────────
if(WIN32)
    list(APPEND SOURCES resources/netanalysis.rc)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"${CMAKE_SOURCE_DIR}/resources\"")
endif()
	if(IOS)
		list(APPEND SOURCES
			src/engine/diagnostics/G1/G1Common.mm
			src/engine/diagnostics/G3/G3Common.mm src/engine/diagnostics/G5/G5Common.mm
			src/platform/PlatformShare.mm src/platform/PlatformStore.mm
			src/platform/PlatformPdfRenderer_ios.mm
	)
	endif()
	if(ANDROID)
		list(APPEND SOURCES
			src/engine/diagnostics/G5/G5Common.cpp src/platform/PlatformShare.cpp
			src/platform/PlatformPdfRenderer_android.cpp
	)
	endif()
	# ---- Native PDF renderer --- available on all platforms --------------
	# 5WHY: NativePdfDocument was mobile-only. Now all platforms get native PDF.
	# macOS: PDFKit, iOS: CGPDFDocument, Android: PdfRenderer via JNI.
	# Windows/Linux: QtPdf if available, else stubs (QML Image fallback).
	list(APPEND SOURCES src/platform/NativePdfDocument.cpp)
	if(APPLE AND NOT IOS)
		list(APPEND SOURCES src/platform/PlatformPdfRenderer_macos.mm)
	elseif(IOS)
		# PlatformPdfRenderer_ios.mm already in if(IOS) block above
	elseif(ANDROID)
		# PlatformPdfRenderer_android.cpp already handled above
	else()
		# Windows / Linux -- stub renderer (QtPdf or image fallback)
		list(APPEND SOURCES src/platform/PlatformPdfRenderer.cpp)
	endif()












set(QML_RESOURCES resources/resources.qrc)

# ── QtWebView QRC — only included when QtWebView module is available ──
# 5WHY: HtmlPreviewWebView.qml contains import QtWebView. If this file
# is in the main QRC, qt_import_qml_plugins() scans it at build time
# and requires the QtWebView plugin — which is unavailable in static
# MSYS2 builds, causing a startup crash. By isolating it in a separate
# QRC that is only bundled when QtWebView exists, static builds skip
# the import and rely on the Image fallback in ReportScreen.qml.
if(TARGET Qt6::WebView)
    list(APPEND QML_RESOURCES resources/resources_webview.qrc)
    message(STATUS "WebView QRC included — in-app HTML preview enabled")
else()
    message(STATUS "WebView QRC excluded — in-app HTML preview uses image fallback")
endif()

# ── QtPdf QRC — only included when QtPdf module is available (Qt 6.4+) ─
# 5WHY: PdfPreviewView.qml contains import QtQuick.Pdf. Same static-build
# constraint as HtmlPreviewWebView.qml — isolate to prevent qmlimportscanner
# from requiring the QtQuick.Pdf plugin in builds without the module.
if(TARGET Qt6::Pdf AND TARGET Qt6::PdfQuick)
    list(APPEND QML_RESOURCES resources/resources_qtpdf.qrc)
    message(STATUS "QtPdf QRC included — in-app PDF preview enabled")
else()
    message(STATUS "QtPdf QRC excluded — PDF preview uses image fallback")
endif()

# ── Native PDF QRC — macOS/iOS/Android have built-in PDF renderers ──────
# 5WHY: NativePdfPageView.qml references NetDiagnostics 1.0 QML type
# (NativePdfDocument). Previously mobile-only; now macOS also has a native
# renderer (PDFKit). Windows/Linux use QtPdf or image fallback.
# Platform renderers: macOS PDFKit, iOS CGPDFDocument, Android PdfRenderer.
if(APPLE OR ANDROID)
    list(APPEND QML_RESOURCES resources/resources_nativepdf.qrc)
    message(STATUS "Native PDF QRC included — platform PDF preview enabled")
else()
    message(STATUS "Native PDF QRC excluded — use QtPdf or image fallback")
endif()

# ── Production target ──────────────────────────────────────────────────
qt_add_executable(net_diagnostics WIN32 ${SOURCES} ${QML_RESOURCES})
configure_netdiag_target(net_diagnostics)
setup_platform_bundle(net_diagnostics)

# ── Simulator target ───────────────────────────────────────────────────
option(ND_DEBUG "Enable crash diagnostic startup logging" OFF)
option(ND_TESTING "Enable automated headless testing harness" OFF)
option(BUILD_SIMULATOR "Build simulator variant" ON)
if(BUILD_SIMULATOR)
    set(SIM_SOURCES ${SOURCES})
    list(REMOVE_ITEM SIM_SOURCES "src/main.cpp")
    # Validate removal
    list(FIND SOURCES "src/main.cpp" _main_cpp_idx)
    if(_main_cpp_idx GREATER -1)
        list(FIND SIM_SOURCES "src/main.cpp" _still_there)
        if(_still_there GREATER -1)
            message(FATAL_ERROR "Failed to remove src/main.cpp from SIM_SOURCES — path mismatch?")
        endif()
    endif()
    list(APPEND SIM_SOURCES "src/main_simulator.cpp")
    # Simulator modules — header-only (.h) with Q_OBJECT macros.
    # Listed as SOURCES so CMake AUTOMOC generates moc_*.cpp for them.
    # CMake does NOT compile .h files; it only passes them through moc.
    list(APPEND SIM_SOURCES
        "src/simulator/SimulatorConfig.h"
        "src/simulator/QtConfigAdapter.h"
        "src/simulator/ScreenshotService.h"
        "src/simulator/MatrixOrchestrator.h"
    )
    qt_add_executable(net_diagnostics_sim WIN32 ${SIM_SOURCES} ${QML_RESOURCES})
    configure_netdiag_target(net_diagnostics_sim)
endif()

# ── Tests ──────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build test suite" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
