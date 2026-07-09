# ── dependencies.cmake ──────────────────────────────────────────────────
# Qt6 + curl discovery and platform SDK setup.
# Included from root CMakeLists.txt.
# ─────────────────────────────────────────────────────────────────────────

# ── Qt6 ─────────────────────────────────────────────────────────────────
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

# ── curl (G5 HTTP diagnostics) ─────────────────────────────────────────
# NO_CURL cmake option (default OFF): exclude libcurl — socket-only G5 tests
# still work (tcpConnect, serviceBanner, ftp, ssh, email, telnet, …).
# iOS/Android cross-compile: find_package(CURL) cannot auto-locate the SDK libcurl
option(NO_CURL "Build without libcurl (skip HTTP-specific G5 tests)" OFF)

if(NO_CURL)
    message(WARNING "Building without curl (NO_CURL=ON) — HTTP-specific diagnostics skipped")
    add_compile_definitions(NO_CURL)
elseif(IOS)
    # iOS: NSURLSession replaces libcurl for HTTP diagnostics (see IosHttpTask.mm)
    set(NO_CURL TRUE)
    add_compile_definitions(NO_CURL)
elseif(ANDROID)
    # Android: vcpkg curl cross-compile is slow/unreliable; skip HTTP diagnostics
    message(WARNING "Android: building without curl (HTTP diagnostics disabled)")
    set(NO_CURL TRUE)
    add_compile_definitions(NO_CURL)
else()
    # Desktop: curl is required for full G5 diagnostics.
    # MODULE forces CMake's built-in FindCURL.cmake (which respects
    # CURL_STATICLIB), bypassing any system CURLConfig.cmake that
    # might link the DLL import library.
    find_package(CURL REQUIRED MODULE)
endif()

# ── iOS SDK detection ───────────────────────────────────────────────────
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
