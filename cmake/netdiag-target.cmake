# ── netdiag-target.cmake ────────────────────────────────────────────────
# Shared target configuration for net_diagnostics.
#
# Usage: configure_netdiag_target(target_name)
# ─────────────────────────────────────────────────────────────────────────

function(configure_netdiag_target TARGET)
    # ── Compile definitions ──────────────────────────────────────────
    target_compile_definitions(${TARGET} PRIVATE
        APP_EDITION="${APP_EDITION}"
        PROJECT_VERSION="${PROJECT_VERSION}"
    )
    # 5WHY: Static Qt builds need Q_IMPORT_PLUGIN for the platform plugin
    # (e.g. QWindowsIntegrationPlugin on Windows).  ND_STATIC_QT tells
    # main.cpp to compile these imports.  Set via build-static.ps1 -Debug.
    if(ND_STATIC_QT)
        target_compile_definitions(${TARGET} PRIVATE ND_STATIC_QT)
        if(WIN32)
            # The static Windows platform plugin library is needed for
            # QWindowsIntegrationPlugin to initialize.  Without this, Qt
            # reports "no Qt platform plugin could be initialized".
            if(NOT TARGET Qt6::QWindowsIntegrationPlugin)
                message(FATAL_ERROR
                    "ND_STATIC_QT requires Qt6::QWindowsIntegrationPlugin from the static Qt installation.")
            endif()
            target_link_libraries(${TARGET} PRIVATE Qt6::QWindowsIntegrationPlugin)
        endif()
    endif()
    if(ND_DEBUG)
        target_compile_definitions(${TARGET} PRIVATE ND_DEBUG)
    endif()
    if(ND_TESTING)
        target_compile_definitions(${TARGET} PRIVATE ND_TESTING)
    endif()
    if(DEFINED ND_BUILD_NUMBER)
        target_compile_definitions(${TARGET} PRIVATE ND_BUILD_NUMBER="${ND_BUILD_NUMBER}")
    else()
        # 5WHY: Local developer builds don't pass -DND_BUILD_NUMBER.
        # Fall back to "0" so the app still compiles and runs.
        set(ND_BUILD_NUMBER "0")
        target_compile_definitions(${TARGET} PRIVATE ND_BUILD_NUMBER="${ND_BUILD_NUMBER}")
    endif()
    if(DEFINED ND_GIT_HASH)
        target_compile_definitions(${TARGET} PRIVATE ND_GIT_HASH="${ND_GIT_HASH}")
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
            if(NOT ND_STRICT_STATIC_WINDOWS)
                target_link_libraries(${TARGET} PRIVATE
                    "-Wl,-Bdynamic"
                )
            endif()
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
        target_compile_definitions(${TARGET} PRIVATE PLATFORM_IOS PLATFORM_MOBILE)
        target_compile_options(${TARGET} PRIVATE
            -F "${IOS_FRAMEWORKS_DIR}"
            -fobjc-arc)  # ensure ARC manages dispatch/NS objects in .mm files
        target_link_options(${TARGET} PRIVATE -F "${IOS_FRAMEWORKS_DIR}")
        target_link_libraries(${TARGET} PRIVATE
            "-framework NetworkExtension"
            "-framework CoreLocation"
            "-framework CoreTelephony"
            "-framework Network"
            "-framework CFNetwork"
            "-framework SystemConfiguration"
            "-framework StoreKit"
            "-framework ReplayKit"
            "-framework AVFoundation"
            "-framework CoreMedia"
        )
    elseif(ANDROID)
        target_compile_definitions(${TARGET} PRIVATE PLATFORM_ANDROID PLATFORM_MOBILE)
        # jnigraphics: AndroidBitmap_* functions used by PlatformPdfRenderer_android.cpp
        target_link_libraries(${TARGET} PRIVATE jnigraphics)
        # liblog: __android_log_print() used by StartupLog.h (logcat mirror)
        target_link_libraries(${TARGET} PRIVATE log)
        # ND_ANDROID_PACKAGE — single source of truth from the manifest template.
        # 5WHY (Android no-log bug): AndroidLogPaths.h needs the package name
        # to build the user-visible external log dir WITHOUT JNI, so even the
        # earliest native STARTUP_LOG (pre-QGuiApplication) lands in
        # /storage/emulated/0/Android/data/<pkg>/files instead of the private
        # cache.  Extract it from AndroidManifest.xml.in so it never drifts
        # from the manifest's package attribute.
        file(READ "${CMAKE_SOURCE_DIR}/resources/android/AndroidManifest.xml.in" _ND_MANIFEST_TXT)
        if(_ND_MANIFEST_TXT MATCHES "package=\"([A-Za-z0-9_.]+)\"")
            set(_ND_ANDROID_PACKAGE "${CMAKE_MATCH_1}")
        else()
            set(_ND_ANDROID_PACKAGE "com.netdiagnostic.app")
        endif()
        target_compile_definitions(${TARGET} PRIVATE
            ND_ANDROID_PACKAGE="${_ND_ANDROID_PACKAGE}")
    endif()

    # ── macOS PDFKit framework ───────────────────────────────────────
    # 5WHY: PDFKit was linked in setup_platform_bundle() which is only
    # called for the production target.  PlatformPdfRenderer.mm uses
    # PDFDocument.  Moved here so all targets link against it.
    if(APPLE AND NOT IOS)
        find_library(PDFKIT PDFKit REQUIRED)
        target_link_libraries(${TARGET} PRIVATE ${PDFKIT})
        # 5WHY: CoreWLAN used by WifiHelper.mm.
        find_library(COREWLAN CoreWLAN REQUIRED)
        target_link_libraries(${TARGET} PRIVATE ${COREWLAN})
        # 5WHY: StoreKit used by PlatformStore.mm (Premium restore/purchase).
        # ARC required for the dispatch/NS objects in the .mm bridge.
        find_library(STOREKIT StoreKit REQUIRED)
        target_link_libraries(${TARGET} PRIVATE ${STOREKIT})
        target_compile_options(${TARGET} PRIVATE -fobjc-arc)
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
    #
    # 5WHY (round 3): This manual import+finalize is only deterministic if
    # the target was created with qt_add_executable(... MANUAL_FINALIZATION).
    # Without it, CMake >= 3.19 defers auto-finalization to end of directory
    # scope, so the qml_plugin_import.cpp could be generated by the deferred
    # auto-finalization at a point where our qt_import_qml_plugins() ordering
    # no longer applies.  MANUAL_FINALIZATION makes the call below the single
    # authoritative finalization step.  See CMakeLists.txt qt_add_executable().
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
endfunction()

# ── Platform bundle setup ─────────────────────────────────────────────
# Called after target creation for platform-specific packaging

function(setup_platform_bundle TARGET)
    # Android APK
    if(ANDROID)
        # ── Version injection ────────────────────────────────────────────
        # 5WHY: resources/android/AndroidManifest.xml hardcoded versionCode=1 /
        # versionName=0.0.1, so every APK reported 0.0.1 regardless of the
        # release tag (v0.0.3).  versionName is what users see; versionCode is
        # what Android uses to decide whether an install is an upgrade.  Both
        # must track PROJECT_VERSION or the APK always reports a stale version.
        #
        # Derive a monotonically increasing versionCode from semver
        # (major*1_000_000 + minor*1_000 + patch):
        #   0.0.3 -> 3, 0.1.0 -> 1000, 1.2.3 -> 1002003.
        set(ANDROID_VERSION_CODE 1)
        if(PROJECT_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
            math(EXPR ANDROID_VERSION_CODE
                "${CMAKE_MATCH_1} * 1000000 + ${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")
        endif()
        set(ANDROID_VERSION_NAME "${PROJECT_VERSION}")

        # 5WHY: QT_ANDROID_PACKAGE_SOURCE_DIR pointed at the source tree, so
        # the manifest could not be generated without dirtying the checkout.
        # Copy the Android package scaffolding (res/, xml/) into the build dir
        # and generate the versioned manifest there; androiddeployqt reads it
        # from this directory at `--target apk` time.
        set(ANDROID_PKG_DIR "${CMAKE_BINARY_DIR}/android")
        file(COPY "${CMAKE_SOURCE_DIR}/resources/android/"
             DESTINATION "${ANDROID_PKG_DIR}")
        configure_file(
            "${CMAKE_SOURCE_DIR}/resources/android/AndroidManifest.xml.in"
            "${ANDROID_PKG_DIR}/AndroidManifest.xml"
            @ONLY)
        # 5WHY: file(COPY) is a one-shot CONFIGURE-time copy with NO dependency
        # tracking — a developer editing a Java/res source under
        # resources/android/ would keep building the stale copy in
        # ${ANDROID_PKG_DIR} with no warning.  configure_file() already tracks
        # the manifest template; this glob registers the rest of the package
        # source tree as a re-configure trigger so edits are re-copied at the
        # next build.
        file(GLOB_RECURSE _ND_PKG_SOURCE_FILES CONFIGURE_DEPENDS
             "${CMAKE_SOURCE_DIR}/resources/android/*")

        set_target_properties(${TARGET} PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${ANDROID_PKG_DIR}"
        )
        # androiddeployqt's generated Gradle release task emits an unsigned
        # APK for this Qt 6.5.3 build. The CI action signs it explicitly with
        # zipalign + apksigner after packaging, keeping signing credentials
        # outside CMake and the generated Gradle project.
    endif()

    # macOS desktop .app bundle
    if(APPLE AND NOT IOS)
        set_target_properties(${TARGET} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.netdiagnostic.app"
            MACOSX_BUNDLE_BUNDLE_VERSION "${ND_BUILD_NUMBER}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/apple/Info-macos.plist"
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
        # 5WHY: com.apple.application-identifier and com.apple.developer.team-identifier
        # are DERIVED entitlements auto-injected by codesign.  The template
        # intentionally omits them — no @APPLE_TEAM_ID@ placeholder needed.
        # configure_file() copies the template to the build dir (dormant @ONLY
        # substitution — left as forward-compat scaffolding if a placeholder is
        # ever added back for non-App-Store signing flows).
        if(NOT DEFINED APPLE_TEAM_ID)
            set(APPLE_TEAM_ID "0000000000")  # dormant — template has no @VAR@
        endif()
        configure_file(
            "${CMAKE_SOURCE_DIR}/resources/apple/macos.entitlements.in"
            "${CMAKE_BINARY_DIR}/macos.entitlements"
            @ONLY
        )
        # PDFKit and CoreWLAN linking moved to configure_netdiag_target.
        # See 5WHY comment there for rationale.
    endif()

    # iOS .app bundle
    if(IOS)
        set_target_properties(${TARGET} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/apple/Info.plist"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.netdiagnostic.app"
            MACOSX_BUNDLE_BUNDLE_NAME "NetDiagnostics"
            MACOSX_BUNDLE_BUNDLE_VERSION "${ND_BUILD_NUMBER}"
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
                XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/resources/apple/netdiagnostic.entitlements"
            )
        endif()
    endif()
endfunction()
