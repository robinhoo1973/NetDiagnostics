#if defined(PLATFORM_MOBILE)
#include <QGuiApplication>
#else
#include <QApplication>
#include <QMessageBox>
#endif
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QVariantMap>
#include <QIcon>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include "Common/Utils/Translator.h"
#include <csignal>
#if defined(_WIN32)
#include <windows.h>
// 5WHY: Static Qt on Windows needs explicit platform plugin import.
// Without this, Qt reports "no Qt platform plugin could be initialized".
// ND_STATIC_QT is defined in CMakeLists.txt for static MSYS2 builds.
#if defined(ND_STATIC_QT) && defined(_WIN32)
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif
#endif
#if !defined(NO_CURL)
#include <curl/curl.h>
#endif
#include "app/AppState.h"
#include "Dashboard/Controller/DashboardController.h"
#include "Diagnostics/Controller/DiagnosticsController.h"
#include "Configuration/Controller/ConfigurationController.h"
#include "Report/Controller/ReportController.h"
#include "Settings/Controller/SettingsController.h"
#if defined(__APPLE__) || defined(PLATFORM_ANDROID)
#include "Common/Platform/NativePdfDocument.h"
#endif
#if defined(PLATFORM_ANDROID)
#include "Common/Platform/Android/AndroidLogPaths.h"
#endif
#if defined(PLATFORM_IOS)
#include "Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.h"
#endif
#include "Common/Utils/DebugSwitch.h"
#include "Common/Utils/StartupLog.h"
#include "Common/Utils/CrashHandler.h"
#if defined(ND_TESTING)
#include "Common/Tests/TestHarness.h"
#include "Common/Tests/TestScenarios.h"
#endif

int main(int argc, char *argv[])
{
#if !defined(NO_CURL)
    curl_global_init(CURL_GLOBAL_ALL);
#endif

#if !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif

    // ── Crash handler — install BEFORE QApplication to catch ctor crashes ──
    CrashHandler::install();

    // 5WHY (Android no-log bug): the first STARTUP_LOG used to run only AFTER
    // QGuiApplication, so a crash inside Qt platform-plugin init (or earlier)
    // produced ZERO native output.  Mark main() entry as the earliest native
    // point.  On Android this targets the user-visible external log dir even
    // before JNI is ready (AndroidLogPaths.h package-name fallback) and is
    // mirrored to logcat.  Placed AFTER CrashHandler::install() so a fault
    // inside the logger itself is still captured by the signal handlers.
    STARTUP_LOG("main() entered — native entry, pre-QGuiApplication");

    qputenv("QSG_RENDER_LOOP", "basic");
#if defined(PLATFORM_MOBILE)
    QGuiApplication app(argc, argv);
#if defined(PLATFORM_ANDROID)
    // 5WHY: JNI becomes available only after QGuiApplication fully constructs
    // (Qt Android platform plugin load).  Set the explicit readiness flag NOW
    // — QCoreApplication::instance() is already non-null but JNI is not, so
    // crash/log paths must not enter JNI before this point (see
    // AndroidLogPaths.h).
    setAndroidJniReady(true);
#endif
#else
    QApplication app(argc, argv);

    // ── Single instance guard ──────────────────────────────────────────────
#if defined(_WIN32)
    // Windows: named mutex — OS auto-releases on process death
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"Global\\NetDiagnostic_SingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        // Try to find and activate existing window
        HWND hwnd = FindWindowW(nullptr, L"NetDiagnostics");
        if (hwnd) {
            SetForegroundWindow(hwnd);
            ShowWindow(hwnd, SW_RESTORE);
        }
#if !defined(NO_CURL)
        curl_global_cleanup();
#endif
        return 0;
    }
        // hMutex is owned by this process; auto-released on exit
#else
        // Linux/macOS: QLockFile (Qt 5.10+ stores PID/hostname/appname
        // in the lock file — no separate .pid sidecar needed)
        QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/netdiagnostic.lock");
        QLockFile lockFile(lockPath);
        lockFile.setStaleLockTime(5000);
        if (!lockFile.tryLock(100)) {
            // 5WHY: Previously used a manual .pid sidecar file that
            // duplicated QLockFile's built-in PID storage.  getLockInfo()
            // reads the PID directly from the lock file.
            qint64 pid = -1;
            bool gotInfo = lockFile.getLockInfo(&pid, nullptr, nullptr);
#if !defined(NO_CURL)
            curl_global_cleanup();
#endif
            // Log instead of dialog — QMessageBox requires QApplication which isn't created yet
            if (gotInfo && pid > 0)
                fprintf(stderr, "NetDiagnostics is already running (PID: %lld)\n", pid);
            else
                fprintf(stderr, "NetDiagnostics is already running (PID unknown)\n");
            return 0;
        }
#endif
#endif

    app.setApplicationName("NetDiagnostics");
    app.setApplicationDisplayName("NetDiagnostics");
    app.setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    app.setOrganizationName("robinhoo1973");
    app.setWindowIcon(QIcon(":/icons/netanalysis.ico"));

    // 5WHY (Android launch crash): checkForPreviousCrash() was called BEFORE
    // QGuiApplication was constructed.  On Android crashLogPath() resolves the
    // log dir via androidUserVisibleLogDir() → QJniObject JNI call — but the Qt
    // Android platform plugin (which initializes the JNI environment) is only
    // loaded by QGuiApplication.  Calling QJniObject before that crashed the
    // app at startup, before the first STARTUP_LOG could be written.  Moved the
    // check to AFTER app construction so JNI is always available.
    bool hadCrash = CrashHandler::checkForPreviousCrash();
    STARTUP_LOG("checkForPreviousCrash returned hadCrash=%d, crashPath=%s",
                (int)hadCrash, qPrintable(CrashHandler::crashReportPath()));

    // 5WHY: These early markers bracket the pre-QML init steps (AppState
    // construction, iOS WiFi auth, QML engine construction).  On iOS the log
    // goes to Documents (Files.app), so if the app dies before engine.load()
    // the last marker written pinpoints the failing step — distinguishing an
    // AppState/controller crash from a QML load failure.
    STARTUP_SEPARATOR();
    STARTUP_LOG("main() reached — constructing AppState...");

    AppState appState;
    STARTUP_LOG("AppState constructed OK");

    // 5WHY: Surface a previous-run crash report to QML so the user can share
    // or upload it.  hadCrash was previously detected but never exposed, so
    // the crash log sat in Documents unused.  Wiring the path into AppState
    // lets the UI show a "share crash report" banner (hasCrashReport).
    if (hadCrash)
        appState.setCrashReportPath(CrashHandler::crashReportPath());

    // 5WHY: On iOS, WiFi authorization (CLLocationManager WhenInUse) must be
    // requested early so NEHotspotNetwork can return SSID/BSSID.  Without this
    // call the WiFi diagnostics panel shows blank data.  The authorization
    // prompt is shown only once; subsequent calls are no-ops.
#if defined(PLATFORM_IOS)
    STARTUP_LOG("Requesting iOS WiFi authorization...");
    iosRequestWiFiAuthorization();
    STARTUP_LOG("iOS WiFi authorization request returned");
#endif

    STARTUP_LOG("AppState setup complete — constructing QQmlApplicationEngine");
    QQmlApplicationEngine engine;
    STARTUP_LOG("QQmlApplicationEngine constructed OK");

#if defined(ND_BUILD_NUMBER)
#  if defined(ND_GIT_HASH)
    STARTUP_LOG("NetDiagnostics starting, Qt %s, edition=%s, build=%s, git=%s",
                qVersion(), APP_EDITION, ND_BUILD_NUMBER, ND_GIT_HASH);
#  else
    STARTUP_LOG("NetDiagnostics starting, Qt %s, edition=%s, build=%s",
                qVersion(), APP_EDITION, ND_BUILD_NUMBER);
#  endif
#else
    STARTUP_LOG("NetDiagnostics starting, Qt %s, edition=%s",
                qVersion(), APP_EDITION);
#endif
    MAIN_LOG(" NetDiagnostics starting, Qt %s\n", qVersion());

    STARTUP_LOG("Setting context properties: appState, controllers, models...");
    // Theme now handled by ThemeEngine.qml singleton — no C++ injection needed
    engine.rootContext()->setContextProperty("appState", &appState);
    // MVC Controllers + Models — injected for gradual QML migration
    engine.rootContext()->setContextProperty("dashboardCtrl", QVariant::fromValue(static_cast<QObject*>(appState.dashboardController())));
    engine.rootContext()->setContextProperty("diagCtrl", QVariant::fromValue(static_cast<QObject*>(appState.diagnosticsController())));
    engine.rootContext()->setContextProperty("configCtrl", QVariant::fromValue(static_cast<QObject*>(appState.configurationController())));
    engine.rootContext()->setContextProperty("reportCtrl", QVariant::fromValue(static_cast<QObject*>(appState.reportController())));
    engine.rootContext()->setContextProperty("settingsCtrl", QVariant::fromValue(static_cast<QObject*>(appState.settingsController())));
    engine.rootContext()->setContextProperty("targetModel", QVariant::fromValue(static_cast<QObject*>(appState.targetModel())));
    engine.rootContext()->setContextProperty("resultsModel", QVariant::fromValue(static_cast<QObject*>(appState.resultsModel())));
    STARTUP_LOG("Core context properties set. Loading TranslationsProxy...");
    // ── Reactive translation proxy ────────────────────────────────────
    // 5WHY: exposing the raw C++ Translator as "T" made QML bindings call
    // T.tr()/T.diagName()/... as plain C++ methods — the binding engine
    // never captured a dependency on the language, so switching language
    // left the UI frozen in the old language.  (QQmlProperty::read() does
    // NOT register binding dependencies in Qt 6 — see TranslationsProxy.qml
    // for the full analysis and the standalone verification.)
    // Fix: expose a self-contained QML proxy as "T".  Its functions read
    // root.lang (a QML binding on appState.languageIndex) so calling
    // bindings re-evaluate on language change.  The C++ Translator stays
    // for AppState integration (language sync, kMaxLanguages) but is no
    // longer the QML-facing translation surface.
    // The proxy parses the translations JSON synchronously at creation, so
    // the raw JSON string is provided here (sync XHR on qrc:/ is blocked in
    // Qt 6.8).
    {
        QFile tf(QStringLiteral(":/translations.json"));
        QString translationsJson;
        if (tf.open(QFile::ReadOnly))
            translationsJson = QString::fromUtf8(tf.readAll());
        engine.rootContext()->setContextProperty("TJson", translationsJson);

        QQmlComponent proxyComp(&engine, QUrl(QStringLiteral("qrc:/qml/theme/TranslationsProxy.qml")));
        QObject* translatorProxy = proxyComp.create();
        if (translatorProxy) {
            // Keep the proxy alive for the whole app lifetime (C++ ownership
            // — context properties do not keep QML objects referenced).
            QQmlEngine::setObjectOwnership(translatorProxy, QQmlEngine::CppOwnership);
            engine.rootContext()->setContextProperty("T", translatorProxy);
        } else {
            // Fallback: bindings won't be reactive, but the app still works.
            qWarning().noquote() << "TranslationsProxy creation failed:"
                                 << proxyComp.errorString();
            engine.rootContext()->setContextProperty("T", appState.translator());
        }
    }
    // QtWebView availability flag — QML uses this to avoid import crash
    // on platforms without the WebView module (e.g., static MSYS2 builds).
#if defined(HAS_QTWEBVIEW)
    engine.rootContext()->setContextProperty("hasWebView", true);
#else
    engine.rootContext()->setContextProperty("hasWebView", false);
#endif
    // QtPdf availability flag — QML uses this to show real PDF viewer
    // (PdfMultiPageView) vs. image-based fallback on platforms without QtPdf.
#if defined(HAS_QTPDF)
    engine.rootContext()->setContextProperty("hasQtPdf", true);
#else
    engine.rootContext()->setContextProperty("hasQtPdf", false);
#endif
    // Native PDF rendering — available on all platforms that have a
    // built-in PDF renderer (no extra Qt module needed):
    //   macOS: PDFKit (Quartz framework)
    //   iOS:   CGPDFDocument (CoreGraphics)
    //   Android: PdfRenderer via JNI (API 21+)
    // Windows/Linux: prefer QtPdf (QPdfDocument); NativePdf stays false.
#if defined(__APPLE__) || defined(PLATFORM_ANDROID)
    engine.rootContext()->setContextProperty("hasNativePdf", true);
    qmlRegisterType<NativePdfDocument>("NetDiagnostics", 1, 0, "NativePdfDocument");
#else
    engine.rootContext()->setContextProperty("hasNativePdf", false);
#endif
    STARTUP_LOG("Context properties set. Loading QML: %s", "qrc:/qml/main.qml");

    // Capture QML warnings/errors to the startup log
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
        &engine, [](const QList<QQmlError>& warnings) {
            for (const auto& w : warnings) {
                QByteArray warnUtf8 = w.toString().toUtf8();
                STARTUP_LOG("QML WARNING: %s", warnUtf8.constData());
            }
        });

    const QUrl url("qrc:/qml/main.qml");

    // Headless auto-run: if ND_AUTORUN=1, auto-set target and run all tests
    if (qEnvironmentVariableIntValue("ND_AUTORUN")) {
        QTimer::singleShot(3000, &app, [&appState]() {
            appState.setTarget(QStringLiteral("localhost"));
            appState.runDiagnostics();
        });
    }

    // ── Maximize the window atomically via C++ ───────────────────────────

#if defined(ND_TESTING)
    // ── Headless testing mode: --test runs scenarios, no GUI ──────────
    if (argc >= 2 && strcmp(argv[1], "--test") == 0) {
        QString logPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/netdiag-test-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".log";
        TestHarness::instance().setLogPath(logPath);
        TestHarness::instance().setAppState(&appState);
        TestHarness::instance().logInfo("Headless test mode started");
        TestHarness::instance().logInfo("Log: " + logPath);

        auto scenarios = TestScenarios::ciScenarios();
        TestHarness::instance().logInfo(QString("Running %1 test scenarios").arg(scenarios.size()));
        for (const auto& tc : scenarios)
            TestHarness::instance().runTestCase(tc);

        TestHarness::instance().printSummary();
        int exitCode = TestHarness::instance().allPassed() ? 0 : 1;
        fprintf(stdout, "\nTest results: %d passed, %d failed → exit %d\n",
                TestHarness::instance().passCount(), TestHarness::instance().failCount(), exitCode);
        fflush(stdout);
        return exitCode;
    }
#endif

    // 5WHY: Qt::QueuedConnection meant the handler might not fire before
    // the rootObjects().isEmpty() check below, silently missing the real
    // QML error.  Direct connection ensures the diagnostic is logged
    // immediately when object creation fails during engine.load().
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [url](const QUrl &objUrl) {
            QByteArray fatalUtf8 = objUrl.toString().toUtf8();
            STARTUP_LOG("QML FATAL: object creation failed for %s", fatalUtf8.constData());
            if (url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::DirectConnection);

    STARTUP_LOG("Calling engine.load()...");
    STARTUP_TRACE("calling engine.load(%s)...", qPrintable(url.toString()));
    engine.load(url);
    STARTUP_LOG("engine.load() returned. rootObjects=%d", engine.rootObjects().size());
    STARTUP_TRACE("engine.load() returned, rootObjects=%d", engine.rootObjects().size());

    if (engine.rootObjects().isEmpty()) {
        STARTUP_TRACE("FATAL: rootObjects is empty — QML failed to load");
        // 5WHY: Silent crash on QML load failure — no diagnostic visible to
        // the user on desktop. Previous crash fixes (d220a44, e44de87) added
        // this pattern for QtWebView import failures in static builds.
        // Capture the exact error and show a message box so the user can
        // report the root cause instead of just seeing a flash-and-quit.
        STARTUP_LOG("FATAL: QML engine failed to load %s — no root objects", "qrc:/qml/main.qml");
        qCritical() << "QML engine failed to load" << url;
#if defined(PLATFORM_MOBILE)
        // 5WHY: On iOS/Android there is no QMessageBox (QtWidgets is
        // excluded on mobile).  Do NOT use qFatal() here — it calls abort()
        // which produces a SIGABRT crash report in TestFlight/Play Console,
        // masking the real root cause (QML load failure) as "app crash".
        // Instead, log the full diagnostic via qCritical (stderr → syslog
        // on iOS, logcat on Android) and return -1 for a clean exit.
        // The error is visible in Xcode Console.app / adb logcat and the
        // StartupLog file, enabling support to diagnose missing QML plugins,
        // corrupted QRC resources, or platform-specific module gaps.
        // 5WHY: On iOS the startup log is written to the Documents directory
        // (retrievable via Files.app); on Android it now goes to the
        // app-scoped external dir (Android/data/<pkg>/files, USB/MTP-visible)
        // instead of the invisible private TempLocation cache dir.
#if defined(PLATFORM_IOS)
        const QString logDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
#if defined(PLATFORM_ANDROID)
        const QString logDir = androidUserVisibleLogDir();
#else
        const QString logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#endif
#endif
        qCritical(
            "NetDiagnostics — Startup Error\n\n"
            "Failed to load QML UI from %s.\n\n"
            "Common causes:\n"
            "- QtQuick / QtQuickControls2 plugin missing\n"
            "- Static build missing QML plugins\n"
            "- Corrupted resources.qrc or missing fonts\n\n"
            "Full log: %s/NetDiagnostics_startup.log",
            qPrintable(url.toString()),
            qPrintable(logDir));
        return -1;
#else
        QMessageBox::critical(nullptr, QStringLiteral("NetDiagnostics — Startup Error"),
            QStringLiteral("Failed to load the QML UI.\n\n"
            "This usually means a required Qt module is missing from your installation.\n"
            "Check the startup log at: %1\n\n"
            "Common causes:\n"
            "• QtWebView or QtPdf module not installed\n"
            "• Static build missing QML plugins\n"
            "• Corrupted resources.qrc file")
            .arg(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                 .filePath("NetDiagnostics_startup.log")));
        return -1;
#endif
    }
    STARTUP_LOG("QML loaded successfully. Showing window.");
    STARTUP_TRACE("QML loaded OK, rootObjects=%d", engine.rootObjects().size());

    // 5WHY: The startup log exists only to diagnose launch crashes.
    // Once QML loads + window shows, the app started successfully —
    // delete the log so stale crash-debug logs don't accumulate.
    STARTUP_CLEANUP();

    // ── Maximize the window atomically via C++ ───────────────────────────
    // QML's visibility: Window.Maximized sets the flag after the window is
    // already visible, which some WMs silently ignore for frameless windows.
    // C++ showMaximized() maps the window in maximized state from its first
    // frame — no transient "default-size then maximize" race.
    {
        QQuickWindow *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (win) {
            win->showMaximized();

            // ── Windows taskbar + Alt+Tab visibility for frameless windows ──
            // 5WHY: Qt.FramelessWindowHint removes WS_CAPTION which causes
            // Windows to treat this as a tool window — no taskbar button,
            // no Alt+Tab entry.  The user loses the window on focus loss
            // with no way to switch back.  Fix: add WS_EX_APPWINDOW extended
            // style to make the window appear in the taskbar + Alt+Tab.
            // Additionally, set the window icon on the native HWND so the
            // taskbar button shows the app icon instead of a blank square.
#if defined(_WIN32)
            WId wid = win->winId();
            if (wid) {
                HWND hwnd = reinterpret_cast<HWND>(wid);

                // Force taskbar + Alt+Tab visibility for frameless windows.
                LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                exStyle |= WS_EX_APPWINDOW;  // show in taskbar + Alt+Tab
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

                // Load application icon from embedded resource (ID 1).
                HICON hIcon = LoadIcon(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
                // 5WHY: LoadIcon returns NULL if the .rc resource wasn't linked
                // (e.g. CMake forgot the .rc file, or static build skipped it).
                // Fall back to loading the .ico file directly from disk.
                if (!hIcon) {
                    hIcon = (HICON)LoadImageW(nullptr,
                        L"resources/icons/netanalysis.ico",
                        IMAGE_ICON, 0, 0,
                        LR_LOADFROMFILE | LR_DEFAULTSIZE);
                }
                if (hIcon) {
                    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
                    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
                }
            }
#endif
        }
    }

    STARTUP_TRACE("entering event loop (app.exec)...");
    int ret = app.exec();
    STARTUP_TRACE("event loop exited, ret=%d", ret);
    #if !defined(NO_CURL)
    curl_global_cleanup();
#endif
    return ret;
}