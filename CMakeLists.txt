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
    # iOS: NSURLSession replaces libcurl for HTTP diagnostics (see IosHttpTask.mm)
    # No curl dependency needed — set NO_CURL to exclude libcurl linking
    set(NO_CURL TRUE)
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
    src/util/DnsResolver.cpp
    src/util/DiagnosticFormatter.cpp
    src/engine/task/DiagnosticTask.cpp
    src/engine/task/TaskFactory.cpp
    src/engine/runner/NetworkProbe.cpp
    src/app/AppState.cpp
    src/engine/diagnostic/G1G2G3Native.cpp
    src/engine/diagnostic/G4RemoteHost.cpp
    src/engine/diagnostic/G5WebsiteUrl.cpp
)
# Android: keep G5WebsiteUrl.cpp — urlParsing, tcpConnect, serviceBanner are
# libcurl-free (raw sockets / QUrl) and compiled fine with Android NDK.
# Curl-dependent functions are guarded by #ifndef NO_CURL inside the file.
# iOS keeps G5WebsiteUrl.cpp for non-curl functions (urlParsing, tcpConnect,
# serviceBanner) — IosHttpTask.mm and TaskFactory PLATFORM_IOS branches depend
# on these symbols.
# Windows: embed .ico into the executable
if(WIN32)
    list(APPEND SOURCES resources/netanalysis.rc)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"${CMAKE_SOURCE_DIR}/resources\"")
endif()
# iOS: native task implementations + WiFi helper
if(IOS)
    list(APPEND SOURCES src/engine/IosWiFiHelper.mm)
    list(APPEND SOURCES src/engine/task/IosDnsTask.mm src/engine/task/IosHttpTask.mm src/engine/task/IosNetworkInfo.mm)
    list(APPEND SOURCES src/engine/PlatformShare_ios.mm)
    list(APPEND SOURCES src/engine/PlatformStore_ios.mm)
endif()
# Android: native JNI diagnostics
if(ANDROID)
    list(APPEND SOURCES src/engine/task/AndroidNetworkInfo.cpp)
    list(APPEND SOURCES src/engine/task/PlatformShare_android.cpp)
endif()

set(QML_RESOURCES
    resources/resources.qrc
)

# ── iOS SDK detection (once, shared by both targets) ─────────────────
if(IOS)
    execute_process(COMMAND xcrun --sdk iphoneos --show-sdk-path
        OUTPUT_VARIABLE IOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE XCRUN_RESULT
        ERROR_VARIABLE XCRUN_ERROR)
    if(NOT XCRUN_RESULT EQUAL 0 OR IOS_SDK_PATH STREQUAL "")
        message(FATAL_ERROR "xcrun --sdk iphoneos --show-sdk-path failed (exit ${XCRUN_RESULT}): ${XCRUN_ERROR}\n  iOS builds require Xcode with the iPhoneOS SDK installed.")
    endif()
    set(IOS_FRAMEWORKS_DIR "${IOS_SDK_PATH}/System/Library/Frameworks")
    if(NOT EXISTS "${IOS_FRAMEWORKS_DIR}")
        message(FATAL_ERROR "iOS frameworks directory not found: ${IOS_FRAMEWORKS_DIR}\n  The SDK layout may have changed in this Xcode release.")
    endif()
endif()

qt_add_executable(net_diagnostics WIN32 ${SOURCES} ${QML_RESOURCES})

# Shared target configuration — eliminates ~60 lines of duplication
# between net_diagnostics and net_diagnostics_sim.
function(configure_netdiag_target TARGET)
    target_compile_definitions(${TARGET} PRIVATE APP_EDITION="${APP_EDITION}")
    if(DEFINED ND_BUILD_NUMBER)
        target_compile_definitions(${TARGET} PRIVATE ND_BUILD_NUMBER="${ND_BUILD_NUMBER}")
    endif()
    # Belt-and-suspenders: ensure Windows GUI subsystem (no console window)
    if(WIN32)
        set_target_properties(${TARGET} PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MINGW)
            target_link_options(${TARGET} PRIVATE -mwindows)
        endif()
    endif()

    target_link_libraries(${TARGET} PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::Quick Qt6::QuickControls2
        Qt6::Network
    )

    if(NOT IOS AND NOT ANDROID)
        target_link_libraries(${TARGET} PRIVATE Qt6::Widgets)
    endif()
    if(TARGET CURL::libcurl)
        target_link_libraries(${TARGET} PRIVATE CURL::libcurl)
    endif()

    if(WIN32)
        target_link_libraries(${TARGET} PRIVATE
            ws2_32 winhttp iphlpapi wlanapi dnsapi
            ole32 shell32 user32 gdi32
            rpcrt4 wldap32 crypt32
        )
    elseif(NOT ANDROID)
        # Android Bionic libc includes resolver; all other platforms need -lresolv
        # (iOS 26 SDK has versioned resolver symbols in separate libresolv.tbd)
        target_link_libraries(${TARGET} PRIVATE resolv)
    endif()

    if(IOS)
        target_compile_definitions(${TARGET} PRIVATE PLATFORM_IOS)
        target_compile_options(${TARGET} PRIVATE -F "${IOS_FRAMEWORKS_DIR}")
        target_link_options(${TARGET} PRIVATE -F "${IOS_FRAMEWORKS_DIR}")
        target_link_libraries(${TARGET} PRIVATE
            "-framework NetworkExtension"
            "-framework CoreLocation"
            "-framework CoreTelephony"
            "-framework Network"
            "-framework CFNetwork"
            "-framework SystemConfiguration"
            "-framework StoreKit"
        )
    elseif(ANDROID)
        target_compile_definitions(${TARGET} PRIVATE PLATFORM_ANDROID)
    endif()

    if(NO_CURL)
        target_compile_definitions(${TARGET} PRIVATE NO_CURL)
    elseif(NOT (IOS OR ANDROID))
        target_compile_definitions(${TARGET} PRIVATE CURL_STATICLIB)
    endif()

    target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/src)
endfunction()

configure_netdiag_target(net_diagnostics)

# Android: enable APK target generation (Qt 6.5+ auto-generates apk target
# when QT_ANDROID_PACKAGE_SOURCE_DIR is set)
if(ANDROID)
    set_target_properties(net_diagnostics PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/resources/android"
    )
endif()

# macOS desktop: build as .app bundle
if(APPLE AND NOT IOS)
    set_target_properties(net_diagnostics PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.netdiagnostic.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist"
        XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "26.0"
    )
    # arm64-only requires macOS 26.0+.  LSMinimumSystemVersion and
    # LSApplicationCategoryType are App Store submission requirements.
    if(IOS)
        # iOS uses a separate Info.plist
    else()
        set_target_properties(net_diagnostics PROPERTIES
            XCODE_ATTRIBUTE_LSMinimumSystemVersion "26.0"
            XCODE_ATTRIBUTE_LSApplicationCategoryType "public.app-category.utilities"
        )
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/icons/netanalysis.icns")
        set_target_properties(net_diagnostics PROPERTIES
            MACOSX_BUNDLE_ICON_FILE "netanalysis.icns")
        set_source_files_properties(
            "${CMAKE_SOURCE_DIR}/resources/icons/netanalysis.icns"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
        )
    else()
        message(WARNING "netanalysis.icns not found — macOS app will lack an icon. Run the icon generation step first (see apple.yml).")
    endif()
endif()

# iOS: configure app bundle metadata
if(IOS)
    set_target_properties(net_diagnostics PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.netdiagnostic.app"
        MACOSX_BUNDLE_BUNDLE_NAME "NetDiagnostics"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © Robin Hu. All rights reserved."
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.netdiagnostic.app"
        XCODE_ATTRIBUTE_PRODUCT_NAME "NetDiagnostics"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    )
    # Add asset catalog to sources for Xcode
    target_sources(net_diagnostics PRIVATE
        "${CMAKE_SOURCE_DIR}/resources/Assets.xcassets"
    )
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/resources/Assets.xcassets"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )
    # Code signing: manual in CI, automatic on developer machines
    if(DEFINED ENV{IOS_TEAM_ID})
        set_target_properties(net_diagnostics PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "$ENV{IOS_TEAM_ID}"
        )
    else()
        set_target_properties(net_diagnostics PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        )
    endif()
    # Code-sign entitlements (opt-in via ND_WIFI_ENTITLEMENT=1 env var)
    # Requires "Access WiFi Information" capability enabled on the App ID.
    # Enable manually: https://developer.apple.com/account/resources/identifiers/list
    if(DEFINED ENV{ND_WIFI_ENTITLEMENT})
        set_target_properties(net_diagnostics PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/resources/netdiagnostic.entitlements"
        )
    endif()
endif()

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
    # Validate removal — exact string match can silently fail if
    # SOURCES paths are refactored (e.g. to ./src/main.cpp).
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
