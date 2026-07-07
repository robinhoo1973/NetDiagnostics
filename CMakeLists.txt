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
    src/util/PingParser.cpp
    src/util/DnsResolver.cpp
    src/util/DiagnosticFormatter.cpp
    src/engine/task/DiagnosticTask.cpp
    src/engine/task/TaskFactory.cpp
    src/engine/runner/NetworkProbe.cpp
    src/app/AppState.cpp
    src/app/DiagnosticConfig.cpp
    src/app/ReportEngine.cpp
    src/app/PremiumStore.cpp
    src/engine/diagnostic/G1G2G3Native.cpp
    src/engine/diagnostic/G4RemoteHost.cpp
    src/engine/diagnostic/G5WebsiteUrl.cpp
    src/engine/diagnostic/G5ProtocolDiagnostics.cpp
)

# ── Platform-specific sources ──────────────────────────────────────────
if(WIN32)
    list(APPEND SOURCES resources/netanalysis.rc)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"${CMAKE_SOURCE_DIR}/resources\"")
endif()
if(IOS)
    list(APPEND SOURCES
        src/engine/IosWiFiHelper.mm
        src/engine/task/IosDnsTask.mm src/engine/task/IosHttpTask.mm src/engine/task/IosNetworkInfo.mm
        src/engine/PlatformShare_ios.mm src/engine/PlatformStore_ios.mm
    )
endif()
if(ANDROID)
    list(APPEND SOURCES
        src/engine/task/AndroidNetworkInfo.cpp src/engine/task/PlatformShare_android.cpp
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
