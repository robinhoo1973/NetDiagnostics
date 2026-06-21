cmake_minimum_required(VERSION 3.22)
project(NetDiagnostic VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Concurrent Quick QuickControls2 Widgets
    Network
)
# LinguistTools: optional — only needed for .ts translation file compilation
find_package(Qt6LinguistTools QUIET)

set(SOURCES
    src/main.cpp
    src/models/DiagnosticResult.cpp
    src/util/Logger.cpp
    src/util/PingParser.cpp
    src/engine/runner/NetworkProbe.cpp
    src/app/AppState.cpp
    src/app/NativeService.cpp
    src/engine/diagnostic/DiagnosticEngine.cpp
    src/engine/diagnostic/G1G2G3Native.cpp
    src/engine/diagnostic/G4RemoteHost.cpp
    src/engine/diagnostic/G5WebsiteUrl.cpp
)

set(QML_RESOURCES
    resources/resources.qrc
)

qt_add_executable(net_diagnostic ${SOURCES} ${QML_RESOURCES})

target_link_libraries(net_diagnostic PRIVATE
    Qt6::Core Qt6::Concurrent Qt6::Quick Qt6::QuickControls2
    Qt6::Widgets Qt6::Network
    resolv curl
)
if(WIN32)
    target_link_libraries(net_diagnostic PRIVATE
        ws2_32 winhttp iphlpapi wlanapi dnsapi
        ole32 shell32 user32 gdi32
    )
endif()

target_include_directories(net_diagnostic PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

option(BUILD_TESTS "Build test suite" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# ── Simulator target (separate binary with device-frame UI) ──────────────
option(BUILD_SIMULATOR "Build simulator variant" OFF)
if(BUILD_SIMULATOR)
    set(SIM_SOURCES ${SOURCES})
    list(REMOVE_ITEM SIM_SOURCES "src/main.cpp")
    list(APPEND SIM_SOURCES "src/main_simulator.cpp")
    qt_add_executable(net_diagnostic_sim ${SIM_SOURCES} ${QML_RESOURCES})
    target_link_libraries(net_diagnostic_sim PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::Quick Qt6::QuickControls2
        Qt6::Widgets Qt6::Network
        resolv curl
    )
    if(WIN32)
        target_link_libraries(net_diagnostic_sim PRIVATE
            ws2_32 winhttp iphlpapi wlanapi dnsapi
            ole32 shell32 user32 gdi32
        )
    endif()
    target_include_directories(net_diagnostic_sim PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
endif()
