#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QVariantMap>
#include <QIcon>
#include <QTimer>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <csignal>
#include <curl/curl.h>
#include "app/AppState.h"

int main(int argc, char *argv[])
{
    // Initialize libcurl once (thread-safe on first call only if done before threads)
    curl_global_init(CURL_GLOBAL_ALL);

    // Ignore SIGPIPE — raw ::send() on broken connections returns EPIPE
    // instead of killing the process. The socket code checks errno after
    // every send() call and handles EPIPE gracefully.
    signal(SIGPIPE, SIG_IGN);

    // Use basic (single-threaded) render loop — more stable on ARM64
    qputenv("QSG_RENDER_LOOP", "basic");
    QGuiApplication app(argc, argv);

    // ── Single instance via lock file ────────────────────────────────────
    QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/netanalysis.lock");
    QLockFile lockFile(lockPath);
    if (!lockFile.tryLock(100)) {
        curl_global_cleanup();
        QMessageBox::information(nullptr, QStringLiteral("NetAnalysis"),
            QStringLiteral("NetAnalysis is already running."));
        return 0;
    }

    app.setApplicationName("NetAnalysis");
    app.setApplicationDisplayName("NetAnalysis");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("robinhoo1973");
    app.setWindowIcon(QIcon(":/icons/wifi.svg"));

    AppState appState;

    QQmlApplicationEngine engine;

    fprintf(stderr, "[MAIN] NetDiagnostic starting, Qt %s\n", qVersion());

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
    curl_global_cleanup();
    return ret;
}
