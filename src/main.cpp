#if(defined(PLATFORM_IOS)||defined(PLATFORM_ANDROID))
#include <QGuiApplication>
#else
#include <QApplication>
#include <QMessageBox>
#endif
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QVariantMap>
#include <QIcon>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QLockFile>
#include <csignal>
#if defined(_WIN32)
#include <windows.h>
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
    // Check for crash report from previous run
    bool hadCrash = CrashHandler::checkForPreviousCrash();

    qputenv("QSG_RENDER_LOOP", "basic");
#if(defined(PLATFORM_IOS)||defined(PLATFORM_ANDROID))
    QGuiApplication app(argc, argv);
#else
    QApplication app(argc, argv);

    // ── Single instance guard ──────────────────────────────────────────────
#if defined(_WIN32)
    // Windows: named mutex — OS auto-releases on process death
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"Global\\NetDiagnostic_SingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
#if !defined(NO_CURL)
        curl_global_cleanup();
#endif
        QMessageBox::information(nullptr, QStringLiteral("NetDiagnostics"),
            QStringLiteral("NetDiagnostics is already running."));
        return 0;
    }
        // hMutex is owned by this process; auto-released on exit
#else
        // Linux/macOS: QLockFile with PID fallback
        QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/netdiagnostic.lock");
        QLockFile lockFile(lockPath);
        lockFile.setStaleLockTime(5000);
        if (!lockFile.tryLock(100)) {
#if !defined(NO_CURL)
                curl_global_cleanup();
#endif
                QMessageBox::information(nullptr, QStringLiteral("NetDiagnostics"),
                        QStringLiteral("NetDiagnostics is already running."));
                return 0;
        }
#endif
#endif

    app.setApplicationName("NetDiagnostics");
    app.setApplicationDisplayName("NetDiagnostics");
    app.setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    app.setOrganizationName("robinhoo1973");
    app.setWindowIcon(QIcon(":/icons/netanalysis.ico"));

    AppState appState;

    // 5WHY: On iOS, WiFi authorization (CLLocationManager WhenInUse) must be
    // requested early so NEHotspotNetwork can return SSID/BSSID.  Without this
    // call the WiFi diagnostics panel shows blank data.  The authorization
    // prompt is shown only once; subsequent calls are no-ops.
#if defined(PLATFORM_IOS)
    iosRequestWiFiAuthorization();
#endif

    QQmlApplicationEngine engine;

    STARTUP_SEPARATOR();
#if defined(ND_BUILD_NUMBER)
    STARTUP_LOG("NetDiagnostics starting, Qt %s, edition=%s, build=%s",
                qVersion(), APP_EDITION, ND_BUILD_NUMBER);
#else
    STARTUP_LOG("NetDiagnostics starting, Qt %s, edition=%s",
                qVersion(), APP_EDITION);
#endif
    MAIN_LOG(" NetDiagnostics starting, Qt %s\n", qVersion());

    // Theme now handled by ThemeEngine.qml singleton — no C++ injection needed
    engine.rootContext()->setContextProperty("appState", &appState);
    // MVC Controllers — injected for gradual QML migration to page-specific controllers
    engine.rootContext()->setContextProperty("dashboardCtrl", QVariant::fromValue(static_cast<QObject*>(appState.dashboardController())));
    engine.rootContext()->setContextProperty("diagCtrl", QVariant::fromValue(static_cast<QObject*>(appState.diagnosticsController())));
    engine.rootContext()->setContextProperty("configCtrl", QVariant::fromValue(static_cast<QObject*>(appState.configurationController())));
    engine.rootContext()->setContextProperty("reportCtrl", QVariant::fromValue(static_cast<QObject*>(appState.reportController())));
    engine.rootContext()->setContextProperty("settingsCtrl", QVariant::fromValue(static_cast<QObject*>(appState.settingsController())));
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
            for (const auto& w : warnings)
                STARTUP_LOG("QML WARNING: %s", w.toString().toUtf8().constData());
        });

    const QUrl url("qrc:/qml/main.qml");

    // Headless auto-run: if ND_AUTORUN=1, auto-set target and run all tests
    if (qEnvironmentVariableIntValue("ND_AUTORUN")) {
        QTimer::singleShot(3000, &app, [&appState]() {
            appState.setTarget(QStringLiteral("localhost"));
            appState.runDiagnostics();
        });
    }

    // ── CI Auto-Test mode: ND_AUTO_TEST=1 ──────────────────────────────
    // 5WHY: iOS/Android cannot be tested headlessly in CI because the QML
    // engine needs a screen.  ND_AUTO_TEST runs the full diagnostic suite
    // in GUI mode and auto-exports a report.  With ND_AUTO_TEST_EXIT_CODE=1
    // the process exit code reflects test status for CI integration.
    //
    //   ND_AUTO_TEST=1            → run G1-G4 on localhost
    //   ND_AUTO_TEST_TARGET=<t>   → also run G5 on target <t>
    //   ND_AUTO_TEST_KEEP_OPEN=1  → keep window after completion (manual)
    //   ND_AUTO_TEST_EXIT_CODE=1  → exit(1) if any test fails
    if (qEnvironmentVariableIntValue("ND_AUTO_TEST")) {
        QTimer::singleShot(3000, &app, [&appState]() {
            QString target = qEnvironmentVariable("ND_AUTO_TEST_TARGET");
            if (target.isEmpty()) target = QStringLiteral("localhost");

            // 5WHY: Connect runStatusChanged BEFORE runDiagnostics().
            // If diagnostics complete very quickly (<100ms, e.g. all
            // localhost tests skipped), the signal could fire before
            // the connect is established, missing the completion event.
            bool keepOpen = qEnvironmentVariableIntValue("ND_AUTO_TEST_KEEP_OPEN");
            if (!keepOpen) {
                // 5WHY: Capture &app as context so the connection is
                // auto-disconnected when app is destroyed.  On Linux/GCC
                // the context overload may fail to compile; use explicit
                // capture + disconnect on exit instead.
                QObject::connect(&appState, &AppState::runStatusChanged,
                    &app, [&appState, &app]() {
                    if (appState.runStatus() == RunStatus::Completed ||
                        appState.runStatus() == RunStatus::Error ||
                        appState.runStatus() == RunStatus::Cancelled) {
                        QString reportPath = QStandardPaths::writableLocation(
                            QStandardPaths::TempLocation)
                            + "/NetDiagnostics_auto_test.html";
                        appState.exportHtml(reportPath, true);
                        STARTUP_LOG("Auto-test report: %s",
                                    reportPath.toUtf8().constData());
                        int exitCode = 0;
                        if (qEnvironmentVariableIntValue("ND_AUTO_TEST_EXIT_CODE")) {
                            auto stats = appState.groupStats(-1);
                            if (stats["fail"].toInt() > 0 ||
                                stats["error"].toInt() > 0)
                                exitCode = 1;
                        }
                        QCoreApplication::exit(exitCode);
                    }
                });
            }

            appState.setTarget(target);
            for (int g = 0; g < 5; ++g)
                appState.setGroupActive(g, true);
            appState.runDiagnostics();
        });
    }

#if defined(ND_TESTING)
    // ── Headless testing mode: --test runs scenarios, no GUI ──────────
    if (argc >= 2 && strcmp(argv[1], "--test") == 0) {
        QString logPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/netdiag-test-" + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss") + ".log";
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
            STARTUP_LOG("QML FATAL: object creation failed for %s", objUrl.toString().toUtf8().constData());
            if (url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::DirectConnection);

    STARTUP_LOG("Calling engine.load()...");
    engine.load(url);
    STARTUP_LOG("engine.load() returned. rootObjects=%d", engine.rootObjects().size());

    if (engine.rootObjects().isEmpty()) {
        // 5WHY: Silent crash on QML load failure — no diagnostic visible to
        // the user on desktop. Previous crash fixes (d220a44, e44de87) added
        // this pattern for QtWebView import failures in static builds.
        // Capture the exact error and show a message box so the user can
        // report the root cause instead of just seeing a flash-and-quit.
        STARTUP_LOG("FATAL: QML engine failed to load %s — no root objects", "qrc:/qml/main.qml");
        qCritical() << "QML engine failed to load" << url;
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
        // 5WHY: On iOS/Android there is no QMessageBox (QtWidgets is
        // excluded on mobile).  Do NOT use qFatal() here — it calls abort()
        // which produces a SIGABRT crash report in TestFlight/Play Console,
        // masking the real root cause (QML load failure) as "app crash".
        // Instead, log the full diagnostic via qCritical (stderr → syslog
        // on iOS, logcat on Android) and return -1 for a clean exit.
        // The error is visible in Xcode Console.app / adb logcat and the
        // StartupLog file, enabling support to diagnose missing QML plugins,
        // corrupted QRC resources, or platform-specific module gaps.
        qCritical(
            "NetDiagnostics — Startup Error\n\n"
            "Failed to load QML UI from %s.\n\n"
            "Common causes:\n"
            "- QtQuick / QtQuickControls2 plugin missing\n"
            "- Static build missing QML plugins\n"
            "- Corrupted resources.qrc or missing fonts\n\n"
            "Full log: %s/NetDiagnostics_startup.log",
            qPrintable(url.toString()),
            qPrintable(QStandardPaths::writableLocation(QStandardPaths::TempLocation)));
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

    // ── Maximize the window atomically via C++ ───────────────────────────
    // QML's visibility: Window.Maximized sets the flag after the window is
    // already visible, which some WMs silently ignore for frameless windows.
    // C++ showMaximized() maps the window in maximized state from its first
    // frame — no transient "default-size then maximize" race.
    {
        QQuickWindow *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (win) {
            win->showMaximized();

            // ── Windows taskbar icon for frameless windows ──────────────
            // Qt.FramelessWindowHint strips native chrome, including the icon that
            // Windows shows on the taskbar button. Set the icon on the native
            // HWND so the taskbar entry matches the application icon.
            // Moved inside the same scope as showMaximized() so winId() is
            // called on an already-realized native window, avoiding a nullptr
            // in Qt 6.8+'s Windows QPA for frameless windows.
#if defined(_WIN32)
            WId wid = win->winId();
            if (wid) {
                HWND hwnd = reinterpret_cast<HWND>(wid);
                HICON hIcon = LoadIcon(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
                if (hIcon) {
                    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
                    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
                }
            }
#endif
        }
    }

    int ret = app.exec();
    #if !defined(NO_CURL)
    curl_global_cleanup();
#endif
    return ret;
}