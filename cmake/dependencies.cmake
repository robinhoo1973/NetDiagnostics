# ── dependencies.cmake ──────────────────────────────────────────────────
# Qt6 + curl discovery and platform SDK setup.
# Included from root CMakeLists.txt.
# ─────────────────────────────────────────────────────────────────────────

# ── Qt6 ─────────────────────────────────────────────────────────────────
if(IOS OR ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS
        Core Concurrent Quick QuickControls2 Network
    )
    find_package(Qt6 COMPONENTS WebView QUIET)
else()
    find_package(Qt6 REQUIRED COMPONENTS
        Core Concurrent Quick QuickControls2 Widgets Network
    )
    # QtWebView: in-app HTML report preview (optional — graceful fallback if missing)
    find_package(Qt6 COMPONENTS WebView QUIET)
    if(Qt6WebView_FOUND)
        message(STATUS "QtWebView found — in-app HTML report preview enabled")
    else()
        message(STATUS "QtWebView not found — in-app HTML preview uses image fallback")
    endif()
    # QtPdf: in-app real PDF preview with page navigation (Qt 6.4+, desktop only)
    find_package(Qt6 COMPONENTS Pdf PdfQuick QUIET)
    if(Qt6Pdf_FOUND AND Qt6PdfQuick_FOUND)
        message(STATUS "QtPdf found — in-app PDF preview with PdfMultiPageView enabled")
    else()
        message(STATUS "QtPdf not found — PDF preview uses image fallback")
    endif()
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
    # Desktop: curl is required for full G5 diagnostics
    # 5WHY: find_package(CURL) does try_compile which fails when using
    # a custom-built true-static libcurl.a (at /c/opt/curl-static) because
    # OpenSSL symbols are not linked in the try_compile test.  When
    # CURL_LIBRARY is passed via -D, create imported target directly
    # to skip the try_compile step entirely.
    if(WIN32 AND DEFINED CURL_LIBRARY AND EXISTS "${CURL_LIBRARY}")
        message(STATUS "Using custom static curl: ${CURL_LIBRARY}")
        add_library(CURL::libcurl STATIC IMPORTED)
        set_target_properties(CURL::libcurl PROPERTIES
            IMPORTED_LOCATION "${CURL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
        )
        set(CURL_FOUND TRUE)
    else()
        find_package(CURL REQUIRED)
    endif()
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
