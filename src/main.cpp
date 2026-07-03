#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
#include <QGuiApplication>
#else
#include <QApplication>
#include <QMessageBox>
#endif
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
    engine.rootContext()->setContextProperty("Theme", theme);

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

    int ret = app.exec();
    #ifndef NO_CURL
    curl_global_cleanup();
#endif
    return ret;
}
