# ── netdiag-target.cmake ────────────────────────────────────────────────
# Shared target configuration for net_diagnostics (production) and
# net_diagnostics_sim (simulator).  Eliminates ~60 lines of duplication.
#
# Usage: configure_netdiag_target(target_name)
# ─────────────────────────────────────────────────────────────────────────

function(configure_netdiag_target TARGET)
    # ── Compile definitions ──────────────────────────────────────────
    target_compile_definitions(${TARGET} PRIVATE
        APP_EDITION="${APP_EDITION}"
        PROJECT_VERSION="${PROJECT_VERSION}"
    )
    if(ND_DEBUG)
        target_compile_definitions(${TARGET} PRIVATE ND_DEBUG)
    endif()
    if(ND_TESTING)
        target_compile_definitions(${TARGET} PRIVATE ND_TESTING)
    endif()
    if(DEFINED ND_BUILD_NUMBER)
        target_compile_definitions(${TARGET} PRIVATE ND_BUILD_NUMBER="${ND_BUILD_NUMBER}")
    endif()

    # ── Windows GUI subsystem ────────────────────────────────────────
    if(WIN32)
        set_target_properties(${TARGET} PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MINGW)
            target_link_options(${TARGET} PRIVATE -mwindows)
        endif()
    endif()

    # ── Qt libraries ─────────────────────────────────────────────────
    target_link_libraries(${TARGET} PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::Quick Qt6::QuickControls2
        Qt6::Network
    )
    if(NOT IOS AND NOT ANDROID)
        target_link_libraries(${TARGET} PRIVATE Qt6::Widgets)
    endif()
    # ── QtWebView (in-app HTML report preview) ────────────────────────
    if(TARGET Qt6::WebView)
        target_link_libraries(${TARGET} PRIVATE Qt6::WebView)
        target_compile_definitions(${TARGET} PRIVATE HAS_QTWEBVIEW)
    endif()
    # ── QtPdf (in-app real PDF preview with page navigation, Qt 6.4+) ─
    if(TARGET Qt6::Pdf AND TARGET Qt6::PdfQuick)
        target_link_libraries(${TARGET} PRIVATE Qt6::Pdf Qt6::PdfQuick)
        target_compile_definitions(${TARGET} PRIVATE HAS_QTPDF)
    endif()

    # ── curl ─────────────────────────────────────────────────────────
    if(TARGET CURL::libcurl)
        # 5WHY: libcurl-4.dll was NOT absorbed by static link because
        # -Wl,-Bstatic was placed AFTER CURL::libcurl in link order.
        # The linker resolved -lcurl to the DLL import library before
        # -Bstatic took effect.  Fix: wrap CURL::libcurl INSIDE the
        # -Bstatic sandwich so -lcurl is resolved to a static archive.
        # Also: MSYS2 curl package may ship libcurl.a as a DLL import
        # lib (not a true static archive).  If libcurl-4.dll still
        # appears after this fix, we must build curl from source with
        # --disable-shared --enable-static, same pattern as Qt source build.
        if(WIN32)
            target_link_libraries(${TARGET} PRIVATE
                "-Wl,-Bstatic"
            )
        endif()
        target_link_libraries(${TARGET} PRIVATE CURL::libcurl)
        if(WIN32)
            # 5WHY: Link order matters with -Wl,-Bstatic + ld.exe (left-to-right).
            # Circular deps require repeating libraries: psl→unistring→iconv.
            # Use the traditional "wrap the archive group" pattern by listing
            # unistring+iconv both before AND after psl so all symbols resolve.
            target_link_libraries(${TARGET} PRIVATE
                ssh2 ssl crypto idn2 unistring iconv
                z brotlidec brotlicommon zstd
                nghttp2 ngtcp2_crypto_ossl ngtcp2 nghttp3 psl
                unistring iconv
            )
            target_link_libraries(${TARGET} PRIVATE
                "-Wl,-Bdynamic"
            )
        endif()
    endif()

    # ── Platform system libraries ────────────────────────────────────
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

    # ── Platform compile definitions + frameworks ────────────────────
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
        # jnigraphics: AndroidBitmap_* functions used by PlatformPdfRenderer_android.cpp
        target_link_libraries(${TARGET} PRIVATE jnigraphics)
    endif()

    # ── macOS PDFKit framework ───────────────────────────────────────
    # 5WHY: PDFKit was linked in setup_platform_bundle() which is only
    # called for the production target.  The simulator target also needs
    # PDFKit (PlatformPdfRenderer.mm uses PDFDocument).  Moved here so
    # both net_diagnostics and net_diagnostics_sim link against it.
    if(APPLE AND NOT IOS)
        find_library(PDFKIT PDFKit REQUIRED)
        target_link_libraries(${TARGET} PRIVATE ${PDFKIT})
    endif()

    # ── curl compile definitions ─────────────────────────────────────
    # NO_CURL handled globally in dependencies.cmake via add_compile_definitions
    # CURL_STATICLIB: only when curl is linked statically (not via DLL import lib)
    if(NOT NO_CURL AND NOT (IOS OR ANDROID))
        # Desktop always links curl statically — __imp_ symbols
        # indicate DLL import lib linkage. CURL_STATICLIB tells the
        # curl headers to use static symbol linkage.
        target_compile_definitions(${TARGET} PRIVATE CURL_STATICLIB)
    endif()

    # ── Include paths ────────────────────────────────────────────────
    # 5WHY: Include paths must be set BEFORE qt_import_qml_plugins /
    # qt_finalize_executable.  The generated qml_plugin_import.cpp is
    # compiled with the target's include directories; if they're set
    # after finalization, the generated file may not find headers from
    # ${CMAKE_SOURCE_DIR}/src and ${CMAKE_SOURCE_DIR}/src/Common.
    target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/Common)

    # ── Static QML plugin import (required for static Qt builds) ──────
    # 5WHY: Without qt_import_qml_plugins(), static Qt builds cannot
    # resolve QML modules (QtQuick, QtQuick.Controls, etc.) at runtime.
    # These functions only exist when Qt was built statically — on
    # dynamic Qt (Homebrew, aqtinstall shared), they are not available
    # and are not needed. Use if(COMMAND) guards for cross-platform safety.
    #
    # 5WHY (round 2): qt_import_qml_plugins must be called BEFORE
    # qt_finalize_executable.  The original code had the reverse order,
    # which meant qt_finalize_executable already processed QML imports
    # internally before the explicit qt_import_qml_plugins call could
    # register them.  On iOS static builds this caused the generated
    # qml_plugin_import.cpp to miss QtQuick.Layouts and other modules,
    # producing a silent QML load failure (rootObjects empty) at runtime.
    # Qt's documented order is: import QML plugins FIRST, then finalize.
    if(COMMAND qt_import_qml_plugins)
        qt_import_qml_plugins(${TARGET})
    elseif(COMMAND qt6_import_qml_plugins)
        qt6_import_qml_plugins(${TARGET})
    endif()
    if(COMMAND qt6_finalize_executable)
        qt6_finalize_executable(${TARGET})
    elseif(COMMAND qt_finalize_executable)
        qt_finalize_executable(${TARGET})
    endif()

    # ── Force static GCC runtimes (after Qt finalize, before g++ specs) ─
    # 5WHY: g++ 16.1.0 on MSYS2 injects -lstdc++ / -lwinpthread at
    # the VERY END of the link command.  /ucrt64/lib/libstdc++.a is
    # itself a DLL import lib (not a true static archive), so even
    # -static-libstdc++ cannot bypass the DLL reference.  The TRUE
    # static libstdc++.a lives inside the GCC versioned directory.
    # -static-libgcc DOES work (libgcc_s_seh-1.dll eliminated).
    # For the remaining libstdc++-6.dll + libwinpthread-1.dll, we
    # bundle them alongside the exe in the build.yml post-processing step.
    # See the "Copy GCC runtime DLLs" step in the Windows Static job.
    if(WIN32)
        target_link_options(${TARGET} PRIVATE
            -static-libgcc
        )
    endif()
endfunction()

# ── Platform bundle setup ─────────────────────────────────────────────
# Called after target creation for platform-specific packaging

function(setup_platform_bundle TARGET)
    # Android APK
    if(ANDROID)
        set_target_properties(${TARGET} PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/resources/android"
        )
    endif()

    # macOS desktop .app bundle
    if(APPLE AND NOT IOS)
        set_target_properties(${TARGET} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.netdiagnostic.app"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info-macos.plist"
            XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "26.0"
            XCODE_ATTRIBUTE_LSMinimumSystemVersion "26.0"
            XCODE_ATTRIBUTE_LSApplicationCategoryType "public.app-category.utilities"
        )
        if(EXISTS "${CMAKE_SOURCE_DIR}/resources/icons/netanalysis.icns")
            set_target_properties(${TARGET} PROPERTIES
                MACOSX_BUNDLE_ICON_FILE "netanalysis.icns")
            target_sources(${TARGET} PRIVATE
                "${CMAKE_SOURCE_DIR}/resources/icons/netanalysis.icns"
            )
            set_source_files_properties(
                "${CMAKE_SOURCE_DIR}/resources/icons/netanalysis.icns"
                PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
            )
        else()
            message(WARNING "netanalysis.icns not found — macOS app will lack an icon. Run the icon generation step first (see apple.yml).")
        endif()
        # PDFKit linking moved to configure_netdiag_target so both
        # production and simulator targets get the framework.
        # See 5WHY comment there for rationale.
    endif()

    # iOS .app bundle
    if(IOS)
        set_target_properties(${TARGET} PROPERTIES
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
        target_sources(${TARGET} PRIVATE
            "${CMAKE_SOURCE_DIR}/resources/Assets.xcassets"
        )
        set_source_files_properties(
            "${CMAKE_SOURCE_DIR}/resources/Assets.xcassets"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
        )
        # Code signing: manual in CI, automatic on developer machines
        if(DEFINED ENV{IOS_TEAM_ID})
            set_target_properties(${TARGET} PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "$ENV{IOS_TEAM_ID}"
            )
        else()
            set_target_properties(${TARGET} PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
            )
        endif()
        if(DEFINED ENV{ND_WIFI_ENTITLEMENT})
            set_target_properties(${TARGET} PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/resources/netdiagnostic.entitlements"
            )
        endif()
    endif()
endfunction()
