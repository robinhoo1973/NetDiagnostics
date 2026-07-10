#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
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
#include <QLockFile>
#include <csignal>
#ifdef _WIN32
#include <windows.h>
#endif
#ifndef NO_CURL
#include <curl/curl.h>
#endif
#include "app/AppState.h"
#include "platform/NativePdfDocument.h"
#include "util/DebugSwitch.h"
#include "util/StartupLog.h"
#ifdef ND_TESTING
#include "testing/TestHarness.h"
#include "testing/TestScenarios.h"
#endif

int main(int argc, char *argv[])
{
#ifndef NO_CURL
    curl_global_init(CURL_GLOBAL_ALL);
#endif

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    qputenv("QSG_RENDER_LOOP", "basic");
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    QGuiApplication app(argc, argv);
#else
    QApplication app(argc, argv);

    // ── Single instance guard ──────────────────────────────────────────────
#ifdef _WIN32
    // Windows: named mutex — OS auto-releases on process death
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"Global\\NetDiagnostic_SingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
#ifndef NO_CURL
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
#ifndef NO_CURL
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

    QQmlApplicationEngine engine;

    STARTUP_SEPARATOR();
#ifdef ND_BUILD_NUMBER
    STARTUP_LOG("NetDiagnostics starting, Qt %s, edition=%s, build=%s",
                qVersion(), APP_EDITION, ND_BUILD_NUMBER);
#else
    STARTUP_LOG("NetDiagnostics starting, Qt %s, edition=%s",
                qVersion(), APP_EDITION);
#endif
    MAIN_LOG(" NetDiagnostics starting, Qt %s\n", qVersion());

    // ── Theme injected directly from C++ (avoids QML component creation failure) ──
    QVariantMap theme;
    theme["bgDark"]       = QStringLiteral("#1E1E2E");
    theme["bgSidebar"]    = QStringLiteral("#252538");
    theme["bgCard"]       = QStringLiteral("#16213E");
    theme["bgInput"]      = QStringLiteral("#2A2A4A");
    theme["textPrimary"]  = QStringLiteral("#E0E0E0");
    theme["textSecondary"]= QStringLiteral("#A0A0B8");
    theme["textMuted"]    = QStringLiteral("#606080");
    theme["accent"]       = QStringLiteral("#E94560");
    theme["accentBlue"]   = QStringLiteral("#0078D4");
    theme["cyan"]         = QStringLiteral("#00BCD4");
    theme["passGreen"]    = QStringLiteral("#4ADE80");
    theme["warnYellow"]   = QStringLiteral("#FACC15");
    theme["failRed"]      = QStringLiteral("#EF4444");
    theme["skipGray"]     = QStringLiteral("#888888");
    theme["infoBlue"]     = QStringLiteral("#0078D4");
    theme["borderCard"]   = QStringLiteral("#3A3A5A");
    theme["borderSubtle"] = QStringLiteral("#2A2A4A");
    theme["borderFocused"]= QStringLiteral("#0078D4");
    theme["radiusCard"]   = 12.0;
    theme["radiusButton"] = 8.0;
    theme["radiusSmall"]  = 6.0;
    theme["sidebarWidth"] = 260.0;
    theme["monoFont"]     = QStringLiteral("JetBrains Mono");
    engine.rootContext()->setContextProperty("Theme", theme);

    engine.rootContext()->setContextProperty("appState", &appState);
    // QtWebView availability flag — QML uses this to avoid import crash
    // on platforms without the WebView module (e.g., static MSYS2 builds).
#ifdef HAS_QTWEBVIEW
    engine.rootContext()->setContextProperty("hasWebView", true);
#else
    engine.rootContext()->setContextProperty("hasWebView", false);
#endif
    // QtPdf availability flag — QML uses this to show real PDF viewer
    // (PdfMultiPageView) vs. image-based fallback on platforms without QtPdf.
#ifdef HAS_QTPDF
    engine.rootContext()->setContextProperty("hasQtPdf", true);
#else
    engine.rootContext()->setContextProperty("hasQtPdf", false);
#endif
    // Native PDF rendering: iOS (CGPDFDocument) / Android (PdfRenderer).
    // Available on all mobile platforms without extra Qt modules.
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
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

#ifdef ND_TESTING
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

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [url](const QUrl &objUrl) {
            STARTUP_LOG("QML FATAL: object creation failed for %s", objUrl.toString().toUtf8().constData());
            if (url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    STARTUP_LOG("Calling engine.load()...");
    engine.load(url);
    STARTUP_LOG("engine.load() returned. rootObjects=%d", engine.rootObjects().size());

    if (engine.rootObjects().isEmpty()) {
        STARTUP_LOG("FATAL: QML engine failed to load %s — no root objects", "qrc:/qml/main.qml");
        qCritical() << "QML engine failed to load" << url;
        return -1;
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
#ifdef _WIN32
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
    #ifndef NO_CURL
    curl_global_cleanup();
#endif
    return ret;
}