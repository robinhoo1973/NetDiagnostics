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
#include "util/DebugSwitch.h"
#ifdef PLATFORM_IOS
#include "engine/IosWiFiHelper.h"
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
    app.setApplicationVersion("0.0.1");
    app.setOrganizationName("robinhoo1973");
    app.setWindowIcon(QIcon(":/icons/app-icon.svg"));

#ifdef PLATFORM_IOS
    // Request WiFi SSID access (iOS 14+ needs location permission)
    iosRequestWiFiAuthorization();
#endif

    AppState appState;

    QQmlApplicationEngine engine;

    MAIN_LOG(" NetDiagnostics starting, Qt %s\n", qVersion());

    // ── ThemeEngine (QML singleton) replaces old C++ Theme QVariantMap ──
    // Theme switching (System/Light/Dark) is now handled by
    // resources/qml/theme/ThemeEngine.qml — a QML singleton with full
    // Material Design 3 token support.  All QML components reference
    // ThemeEngine.colors.* or ThemeEngine.* aliases.
    // The old C++ Theme context property has been removed — the QML
    // singleton provides the same keys with light/dark/system awareness.

    engine.rootContext()->setContextProperty("appState", &appState);
    const QUrl url("qrc:/qml/main.qml");

    // Headless auto-run: if ND_AUTORUN=1, auto-set target and run all tests
    if (qEnvironmentVariableIntValue("ND_AUTORUN")) {
        QTimer::singleShot(3000, &app, [&appState]() {
            appState.setTarget(QStringLiteral("localhost"));
            appState.runDiagnostics();
        });
    }
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [url](const QUrl &objUrl) {
            if (url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML engine failed to load" << url;
        return -1;
    }

    // ── Maximize the window atomically via C++ ───────────────────────────
    // QML's visibility: Window.Maximized sets the flag after the window is
    // already visible, which some WMs silently ignore for frameless windows.
    // C++ showMaximized() maps the window in maximized state from its first
    // frame — no transient "default-size then maximize" race.
    {
        QQuickWindow *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (win) {
            win->showMaximized();
        }
    }

    int ret = app.exec();
    #ifndef NO_CURL
    curl_global_cleanup();
#endif
    return ret;
}