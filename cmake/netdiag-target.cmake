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
        target_link_libraries(${TARGET} PRIVATE CURL::libcurl)
        # 5WHY round 9: "LINKER:-Bstatic" syntax doesn't survive CMake
        # generator transform in CI MinGW toolchain. Use raw -Wl, flags
        # which pass directly to the linker without CMake interpretation.
        if(WIN32)
            target_link_libraries(${TARGET} PRIVATE
                "-Wl,-Bstatic"
            )
            target_link_libraries(${TARGET} PRIVATE
                ssh2 idn2 unistring ssl crypto z brotlidec brotlicommon zstd
                nghttp2 ngtcp2_crypto_ossl ngtcp2 nghttp3 psl
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

    # ── curl compile definitions ─────────────────────────────────────
    # NO_CURL handled globally in dependencies.cmake via add_compile_definitions
    # CURL_STATICLIB: only when curl is linked statically (not via DLL import lib)
    if(NOT NO_CURL AND NOT (IOS OR ANDROID))
        # Desktop always links curl statically — __imp_ symbols
        # indicate DLL import lib linkage. CURL_STATICLIB tells the
        # curl headers to use static symbol linkage.
        target_compile_definitions(${TARGET} PRIVATE CURL_STATICLIB)
    endif()

    # ── Force static GCC runtimes (last, so they override any Qt flags) ─
    # CI custom-built Qt at /c/opt/qt6-install may inject -lstdc++ / -lwinpthread
    # as dynamic libs via its .prl INTERFACE_LINK_LIBRARIES.  Repeating
    # -static-libstdc++ -static-libgcc after ALL libraries ensures these
    # runtime DLLs are absorbed into the static exe.
    if(WIN32)
        target_link_options(${TARGET} PRIVATE
            -static-libstdc++ -static-libgcc
        )
    endif()

    # ── Static QML plugin import (required for static Qt builds) ──────
    # 5WHY: Without qt_import_qml_plugins(), static Qt builds cannot
    # resolve QML modules (QtQuick, QtQuick.Controls, etc.) at runtime.
    # These functions only exist when Qt was built statically — on
    # dynamic Qt (Homebrew, aqtinstall shared), they are not available
    # and are not needed. Use if(COMMAND) guards for cross-platform safety.
    if(COMMAND qt6_finalize_executable)
        qt6_finalize_executable(${TARGET})
    elseif(COMMAND qt_finalize_executable)
        qt_finalize_executable(${TARGET})
    endif()
    if(COMMAND qt_import_qml_plugins)
        qt_import_qml_plugins(${TARGET})
    elseif(COMMAND qt6_import_qml_plugins)
        qt6_import_qml_plugins(${TARGET})
    endif()

    # ── Include paths ────────────────────────────────────────────────
    target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/src)
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
        # 5WHY: PlatformPdfRenderer_macos.mm uses PDFDocument from PDFKit.
        # PDFKit is NOT part of Quartz umbrella framework on macOS — it's
        # standalone. Link PDFKit explicitly to resolve _OBJC_CLASS_$_PDFDocument.
        target_link_options(${TARGET} PRIVATE "-framework PDFKit")
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
