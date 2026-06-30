cmake_minimum_required(VERSION 3.22)
project(NetDiagnostic VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(IOS OR ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS
        Core Concurrent Quick QuickControls2 Network
    )
else()
    find_package(Qt6 REQUIRED COMPONENTS
        Core Concurrent Quick QuickControls2 Widgets Network
    )
endif()
# LinguistTools: optional — only needed for .ts translation file compilation
find_package(Qt6LinguistTools QUIET)

# curl — required for G5 HTTP diagnostics
# iOS/Android cross-compile: find_package(CURL) cannot auto-locate the SDK libcurl
if(IOS)
    # iOS: prefer prebuilt libcurl.a; fallback to SDK -lcurl
    if(NOT CURL_INCLUDE_DIR)
        find_path(CURL_INCLUDE_DIR curl/curl.h
            PATHS "${CMAKE_OSX_SYSROOT}/usr/include"
            NO_DEFAULT_PATH
        )
    endif()
    if(CURL_INCLUDE_DIR AND EXISTS "${CURL_INCLUDE_DIR}/curl/curl.h")
        message(STATUS "iOS curl include: ${CURL_INCLUDE_DIR}")
        if(CURL_LIBRARY)
            message(STATUS "iOS curl lib (prebuilt): ${CURL_LIBRARY}")
            add_library(CURL::libcurl UNKNOWN IMPORTED)
            set_target_properties(CURL::libcurl PROPERTIES
                IMPORTED_LOCATION "${CURL_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
            )
        else()
            # Try to locate system libcurl in the iOS SDK
            find_library(IOS_CURL_LIBRARY curl
                PATHS "${CMAKE_OSX_SYSROOT}/usr/lib"
                NO_DEFAULT_PATH
            )
            if(IOS_CURL_LIBRARY)
                message(STATUS "iOS curl library: ${IOS_CURL_LIBRARY}")
                add_library(CURL::libcurl UNKNOWN IMPORTED)
                set_target_properties(CURL::libcurl PROPERTIES
                    IMPORTED_LOCATION "${IOS_CURL_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
                )
            else()
                message(STATUS "iOS: using -lcurl (link-time resolved by SDK)")
                add_library(CURL::libcurl INTERFACE IMPORTED)
                target_include_directories(CURL::libcurl INTERFACE "${CURL_INCLUDE_DIR}")
                target_link_libraries(CURL::libcurl INTERFACE curl)
            endif()
        endif()
    else()
        message(WARNING "iOS: curl not found; building without HTTP diagnostics")
        set(NO_CURL TRUE)
    endif()
elseif(ANDROID)
    # Android: vcpkg curl cross-compile is slow/unreliable; skip HTTP diagnostics
    message(WARNING "Android: building without curl (HTTP diagnostics disabled)")
    set(NO_CURL TRUE)
else()
    if(NO_CURL)
        message(WARNING "Building without curl (NO_CURL=ON)")
    else()
        find_package(CURL REQUIRED)
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/models/DiagnosticResult.cpp
    src/util/Logger.cpp
    src/util/PingParser.cpp
    src/engine/runner/NetworkProbe.cpp
    src/app/AppState.cpp
    src/engine/diagnostic/DiagnosticEngine.cpp
    src/engine/diagnostic/G1G2G3Native.cpp
    src/engine/diagnostic/G4RemoteHost.cpp
    src/engine/diagnostic/G5WebsiteUrl.cpp
)
# iOS without curl: exclude G5 HTTP diagnostic source
if(NO_CURL)
    list(REMOVE_ITEM SOURCES "src/engine/diagnostic/G5WebsiteUrl.cpp")
endif()
# Windows: embed .ico into the executable
if(WIN32)
    list(APPEND SOURCES resources/netanalysis.rc)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"${CMAKE_SOURCE_DIR}/resources\"")
endif()
# iOS: Objective-C++ WiFi helper (CLLocationManager + NEHotspotNetwork)
if(IOS)
    list(APPEND SOURCES src/engine/IosWiFiHelper.mm)
endif()

set(QML_RESOURCES
    resources/resources.qrc
)

qt_add_executable(net_diagnostic ${SOURCES} ${QML_RESOURCES})

# Android: enable APK target generation (Qt 6.5+ auto-generates apk target
# when QT_ANDROID_PACKAGE_SOURCE_DIR is set)
if(ANDROID)
    set_target_properties(net_diagnostic PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/resources/android"
    )
endif()

# macOS desktop: build as .app bundle
if(APPLE AND NOT IOS)
    set_target_properties(net_diagnostic PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.netdiagnostic.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "netanalysis.icns"
    )
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/resources/icons/netanalysis.icns"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

# iOS: configure app bundle metadata
if(IOS)
    set_target_properties(net_diagnostic PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.netdiagnostic.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © Robin Hu. All rights reserved."
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.netdiagnostic.app"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    )
    # Add asset catalog to sources for Xcode
    target_sources(net_diagnostic PRIVATE
        "${CMAKE_SOURCE_DIR}/resources/Assets.xcassets"
    )
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/resources/Assets.xcassets"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )
    # Code signing: manual in CI, automatic on developer machines
    if(DEFINED ENV{IOS_TEAM_ID})
        set_target_properties(net_diagnostic PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "$ENV{IOS_TEAM_ID}"
        )
    else()
        set_target_properties(net_diagnostic PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        )
    endif()
    # Code-sign entitlements (opt-in via ND_WIFI_ENTITLEMENT=1 env var)
    # Requires "Access WiFi Information" capability enabled on the App ID.
    # Enable manually: https://developer.apple.com/account/resources/identifiers/list
    if(DEFINED ENV{ND_WIFI_ENTITLEMENT})
        set_target_properties(net_diagnostic PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/resources/netdiagnostic.entitlements"
        )
    endif()
endif()

target_link_libraries(net_diagnostic PRIVATE
    Qt6::Core Qt6::Concurrent Qt6::Quick Qt6::QuickControls2
    Qt6::Network
)

# Platform macros for source-level conditional compilation
if(IOS)
    target_compile_definitions(net_diagnostic PRIVATE PLATFORM_IOS)
    # iOS frameworks needed for network diagnostics
    # -G Ninja: find_library handles -F automatically
    # -G Xcode: must explicitly add -F to BOTH compile and link options
    execute_process(COMMAND xcrun --sdk iphoneos --show-sdk-path
        OUTPUT_VARIABLE IOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE XCRUN_RESULT
        ERROR_VARIABLE XCRUN_ERROR)
    if(NOT XCRUN_RESULT EQUAL 0 OR IOS_SDK_PATH STREQUAL "")
        message(FATAL_ERROR "xcrun --sdk iphoneos --show-sdk-path failed (exit ${XCRUN_RESULT}): ${XCRUN_ERROR}\n  iOS builds require Xcode with the iPhoneOS SDK installed.")
    endif()
    set(IOS_FRAMEWORKS_DIR "${IOS_SDK_PATH}/System/Library/Frameworks")

    target_compile_options(net_diagnostic PRIVATE
        -F "${IOS_FRAMEWORKS_DIR}"
    )
    target_link_options(net_diagnostic PRIVATE
        -F "${IOS_FRAMEWORKS_DIR}"
    )
    target_link_libraries(net_diagnostic PRIVATE
        "-framework NetworkExtension"
        "-framework CoreLocation"
        "-framework CoreTelephony"
    )
elseif(ANDROID)
    target_compile_definitions(net_diagnostic PRIVATE PLATFORM_ANDROID)
endif()
if(NO_CURL)
    target_compile_definitions(net_diagnostic PRIVATE NO_CURL)
endif()

if(NOT IOS AND NOT ANDROID)
    target_link_libraries(net_diagnostic PRIVATE Qt6::Widgets)
endif()
if(TARGET CURL::libcurl)
    target_link_libraries(net_diagnostic PRIVATE CURL::libcurl)
endif()
if(WIN32)
    target_link_libraries(net_diagnostic PRIVATE
        ws2_32 winhttp iphlpapi wlanapi dnsapi
        ole32 shell32 user32 gdi32
        rpcrt4 wldap32 crypt32
    )
elseif(NOT ANDROID AND NOT APPLE)
    # Android Bionic libc includes resolver; macOS/iOS have it in libSystem
    target_link_libraries(net_diagnostic PRIVATE resolv)
endif()

# iOS/Android use dynamic system libcurl, CURL_STATICLIB not applicable
if(NOT (IOS OR ANDROID))
    target_compile_definitions(net_diagnostic PRIVATE CURL_STATICLIB)
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

# Simulator target
option(BUILD_SIMULATOR "Build simulator variant" OFF)
if(BUILD_SIMULATOR)
    set(SIM_SOURCES ${SOURCES})
    list(REMOVE_ITEM SIM_SOURCES "src/main.cpp")
    list(APPEND SIM_SOURCES "src/main_simulator.cpp")
    qt_add_executable(net_diagnostic_sim ${SIM_SOURCES} ${QML_RESOURCES})
    target_link_libraries(net_diagnostic_sim PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::Quick Qt6::QuickControls2
        Qt6::Network
    )
    if(NOT IOS AND NOT ANDROID)
        target_link_libraries(net_diagnostic_sim PRIVATE Qt6::Widgets)
    endif()
    if(TARGET CURL::libcurl)
        target_link_libraries(net_diagnostic_sim PRIVATE CURL::libcurl)
    endif()
    if(WIN32)
        target_link_libraries(net_diagnostic_sim PRIVATE
            ws2_32 winhttp iphlpapi wlanapi dnsapi
            ole32 shell32 user32 gdi32
            rpcrt4 wldap32 crypt32
        )
    elseif(NOT ANDROID AND NOT APPLE)
        target_link_libraries(net_diagnostic_sim PRIVATE resolv)
    endif()
    if(NO_CURL)
        target_compile_definitions(net_diagnostic_sim PRIVATE NO_CURL)
    endif()
    if(NOT (IOS OR ANDROID))
        target_compile_definitions(net_diagnostic_sim PRIVATE CURL_STATICLIB)
    endif()
    # iOS simulator: same compile definition and frameworks as main target
    if(IOS)
        target_compile_definitions(net_diagnostic_sim PRIVATE PLATFORM_IOS)
        target_compile_options(net_diagnostic_sim PRIVATE
            -F "${IOS_FRAMEWORKS_DIR}"
        )
        target_link_options(net_diagnostic_sim PRIVATE
            -F "${IOS_FRAMEWORKS_DIR}"
        )
        target_link_libraries(net_diagnostic_sim PRIVATE
            "-framework NetworkExtension"
            "-framework CoreLocation"
            "-framework CoreTelephony"
        )
    endif()
    target_include_directories(net_diagnostic_sim PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
endif()