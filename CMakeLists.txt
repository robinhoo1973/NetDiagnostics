cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED PROJECT_VERSION)
    set(PROJECT_VERSION "0.0.1")
endif()
project(NetDiagnostic VERSION ${PROJECT_VERSION} LANGUAGES CXX)

option(ND_DEBUG "Enable crash diagnostic startup logging" OFF)
option(ND_TESTING "Enable automated headless testing harness" OFF)

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
    src/Common/Model/DiagnosticResult.cpp
    src/Common/Model/CaptureFeatureGate.cpp
    src/Common/Utils/Logger.cpp
    src/Common/Services/DnsResolver.cpp
    src/Common/Services/ServerDbUpdater.cpp
    src/Diagnostics/View/DiagnosticFormatter.cpp
    src/Common/Services/DiagnosticTask.cpp
    src/Diagnostics/Controller/TaskFactory.cpp
    src/Common/Services/ProbeDatabase.cpp
    src/Diagnostics/Model/NetworkProbe.cpp
    src/Diagnostics/Model/GeoProbe.cpp
    src/Diagnostics/Model/ProbeScheduler.cpp
    src/Diagnostics/Model/ProbeExecutor.cpp
    src/Diagnostics/Model/ProbeFeedback.cpp
    src/app/TargetModel.cpp
    src/app/ResultsModel.cpp
    src/app/AppState.cpp
    src/Dashboard/Controller/DashboardController.cpp
    src/Diagnostics/Controller/DiagnosticsController.cpp
    src/Configuration/Controller/ConfigurationController.cpp
    src/Report/Controller/ReportController.cpp
    src/Settings/Controller/SettingsController.cpp
    src/Configuration/Model/DiagnosticConfig.cpp
    src/Report/Model/ReportEngine.cpp
    src/Settings/Model/PremiumStore.cpp
    src/Diagnostics/Model/GCommon.cpp
    src/Diagnostics/Model/G1/G1NetworkAdapters.cpp
    src/Diagnostics/Model/G1/G1ActiveConnections.cpp
    src/Diagnostics/Model/G1/G1CellularInfo.cpp
    src/Diagnostics/Model/G1/G1IpConfiguration.cpp
    src/Diagnostics/Model/G1/G1WifiDiagnostics.cpp
    src/Diagnostics/Model/G1/G1NicAdvanced.cpp
    src/Diagnostics/Model/G1/G1WiredDiagnostics.cpp
    src/Diagnostics/Model/G1/G1DhcpStatus.cpp
    src/Diagnostics/Model/G2/G2RoutingTable.cpp
    src/Diagnostics/Model/G2/G2ArpTable.cpp
    src/Diagnostics/Model/G2/G2NetworkProfile.cpp
    src/Diagnostics/Model/G2/G2TcpSettings.cpp
    src/Diagnostics/Model/G2/G2DefaultGateway.cpp
    src/Diagnostics/Model/G2/G2ProxySettings.cpp
    src/Diagnostics/Model/G3/G3NetskopeStatus.cpp
    src/Diagnostics/Model/G3/G3DnsServers.cpp
    src/Diagnostics/Model/G3/G3DnsCache.cpp
    src/Diagnostics/Model/G3/G3DnsIntegrity.cpp
    src/Diagnostics/Model/G3/G3GeoIPLoc.cpp
    src/Diagnostics/Model/G3/G3InternetConnectivity.cpp
    src/Diagnostics/Model/G4/G4DnsResolution.cpp
    src/Diagnostics/Model/G4/G4Ping.cpp
    src/Diagnostics/Model/G4/G4Traceroute.cpp
    src/Diagnostics/Model/G4/G4PathPing.cpp
    src/Diagnostics/Model/G4/G4PingParser.cpp
    src/Diagnostics/Model/G4/G4MtuDiscovery.cpp
    src/Diagnostics/Model/G5/G5UrlParsing.cpp
    src/Diagnostics/Model/G5/G5TcpConnect.cpp
    src/Diagnostics/Model/G5/G5ServiceBanner.cpp
    src/Diagnostics/Model/G5/G5HttpHeaders.cpp
    src/Diagnostics/Model/G5/G5CurlVerbose.cpp
    src/Diagnostics/Model/G5/G5SecurityHeaders.cpp
    src/Diagnostics/Model/G5/G5SslCertificate.cpp
    src/Diagnostics/Model/G5/G5HttpRedirect.cpp
    src/Diagnostics/Model/G5/G5HttpCompression.cpp
    src/Diagnostics/Model/G5/G5HttpTiming.cpp
    src/Diagnostics/Model/G5/G5FtpDiagnostics.cpp
    src/Diagnostics/Model/G5/G5SshDiagnostics.cpp
    src/Diagnostics/Model/G5/G5EmailDiagnostics.cpp
    src/Diagnostics/Model/G5/G5Telnet.cpp
    src/Diagnostics/Model/G5/G5Mysql.cpp
    src/Diagnostics/Model/G5/G5Postgres.cpp
    src/Diagnostics/Model/G5/G5Redis.cpp
    src/Diagnostics/Model/G5/G5Mongodb.cpp
    src/Diagnostics/Model/G5/G5Ldap.cpp
    src/Diagnostics/Model/G5/G5Mqtt.cpp
)

# ── Automated testing harness (ND_TESTING) ──────────────────────────────
if(ND_TESTING)
    list(APPEND SOURCES
        src/Common/Tests/TestHarness.cpp
        src/Common/Tests/TestScenarios.cpp
    )
endif()

# ── Platform-specific sources ──────────────────────────────────────────
if(WIN32)
    list(APPEND SOURCES resources/netanalysis.rc)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"${CMAKE_SOURCE_DIR}/resources\"")
endif()
if(IOS)
    list(APPEND SOURCES
        src/Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.mm
        src/Diagnostics/Model/G3/Platform/IOS/DnsResolve.mm
        src/Diagnostics/Model/G5/Platform/IOS/HttpDiagnostics.mm
        src/Common/Platform/Apple/IOS/PlatformShare.mm
        src/Common/Platform/Apple/IOS/PlatformStore.mm
        src/Common/Platform/Apple/IOS/PlatformPdfRenderer.mm
	)
endif()
if(ANDROID)
    list(APPEND SOURCES
        src/Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.cpp
        src/Common/Platform/Android/PlatformShare.cpp
        src/Common/Platform/Android/PlatformPdfRenderer.cpp
	)
endif()
	# ---- Native PDF renderer --- available on all platforms --------------
	# 5WHY: NativePdfDocument was mobile-only. Now all platforms get native PDF.
	# macOS: PDFKit, iOS: CGPDFDocument, Android: PdfRenderer via JNI.
	# Windows/Linux: QtPdf if available, else stubs (QML Image fallback).
	list(APPEND SOURCES src/Common/Platform/NativePdfDocument.cpp)
	if(APPLE AND NOT IOS)
    list(APPEND SOURCES src/Common/Platform/Apple/macOS/PlatformPdfRenderer.mm)
    list(APPEND SOURCES src/Common/Platform/Apple/macOS/WifiHelper.mm)
	elseif(IOS)
		# PlatformPdfRenderer_ios.mm already in if(IOS) block above
	elseif(ANDROID)
		# PlatformPdfRenderer_android.cpp already handled above
	else()
		# Windows / Linux -- stub renderer (QtPdf or image fallback)
endif()

set(QML_RESOURCES resources/resources.qrc)

# ── QtWebView QRC ??only included when QtWebView module is available ──
# 5WHY: HtmlPreviewWebView.qml contains import QtWebView. If this file
# is in the main QRC, qt_import_qml_plugins() scans it at build time
# and requires the QtWebView plugin ??which is unavailable in static
# MSYS2 builds, causing a startup crash. By isolating it in a separate
# QRC that is only bundled when QtWebView exists, static builds skip
# the import and rely on the Image fallback in ReportScreen.qml.
if(TARGET Qt6::WebView)
    list(APPEND QML_RESOURCES resources/resources_webview.qrc)
    message(STATUS "WebView QRC included ??in-app HTML preview enabled")
else()
    message(STATUS "WebView QRC excluded ??in-app HTML preview uses image fallback")
endif()

# ── QtPdf QRC ??only included when QtPdf module is available (Qt 6.4+) ─
# 5WHY: PdfPreviewView.qml contains import QtQuick.Pdf. Same static-build
# constraint as HtmlPreviewWebView.qml ??isolate to prevent qmlimportscanner
# from requiring the QtQuick.Pdf plugin in builds without the module.
if(TARGET Qt6::Pdf AND TARGET Qt6::PdfQuick)
    list(APPEND QML_RESOURCES resources/resources_qtpdf.qrc)
    message(STATUS "QtPdf QRC included ??in-app PDF preview enabled")
else()
    message(STATUS "QtPdf QRC excluded ??PDF preview uses image fallback")
endif()

# ── Native PDF QRC ??macOS/iOS/Android have built-in PDF renderers ──────
# 5WHY: NativePdfPageView.qml references NetDiagnostics 1.0 QML type
# (NativePdfDocument). Previously mobile-only; now macOS also has a native
# renderer (PDFKit). Windows/Linux use QtPdf or image fallback.
# Platform renderers: macOS PDFKit, iOS CGPDFDocument, Android PdfRenderer.
if(APPLE OR ANDROID)
    list(APPEND QML_RESOURCES resources/resources_nativepdf.qrc)
    message(STATUS "Native PDF QRC included ??platform PDF preview enabled")
else()
    message(STATUS "Native PDF QRC excluded ??use QtPdf or image fallback")
endif()

# ── Production target ──────────────────────────────────────────────────
qt_add_executable(net_diagnostics WIN32 MANUAL_FINALIZATION ${SOURCES} ${QML_RESOURCES})
# 5WHY: setup_platform_bundle must be called BEFORE configure_netdiag_target
# so MACOSX_BUNDLE and MACOSX_BUNDLE_INFO_PLIST are set before
# qt_finalize_executable runs inside configure_netdiag_target.
# Previously the reverse order meant the iOS bundle properties were not
# visible during QML plugin import scanning, producing an incomplete
# qml_plugin_import.cpp.
setup_platform_bundle(net_diagnostics)
configure_netdiag_target(net_diagnostics)


# ── Tests ──────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build test suite" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()