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
    src/engine/runner/NetworkProbe.cpp
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
    )
endif()
if(ANDROID)
    list(APPEND SOURCES
        src/engine/diagnostics/G5/G5Common.cpp src/platform/PlatformShare.cpp
    )
endif()

set(QML_RESOURCES resources/resources.qrc)

# ── Production target ──────────────────────────────────────────────────
qt_add_executable(net_diagnostics WIN32 ${SOURCES} ${QML_RESOURCES})
configure_netdiag_target(net_diagnostics)
setup_platform_bundle(net_diagnostics)

# ── Simulator target ───────────────────────────────────────────────────
option(ND_DEBUG "Enable crash diagnostic startup logging" OFF)
option(BUILD_SIMULATOR "Build simulator variant" OFF)
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
