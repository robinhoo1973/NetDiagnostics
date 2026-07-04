// =============================================================================
// main_simulator.cpp — Simulator-mode entry point
// =============================================================================
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QVariantMap>
#include <QTimer>
#include <QQuickWindow>
#include <QIcon>
#include <QLockFile>
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QMessageBox>
#endif
#include <QStandardPaths>
#include "app/AppState.h"

int main(int argc, char *argv[])
{
    qputenv("QSG_RENDER_LOOP", "basic");
    QGuiApplication app(argc, argv);
    app.setApplicationName("NetDiagnostics Simulator");

    // ── Single instance via lock file ────────────────────────────────────
    QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/netdiagnostic-sim.lock");
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(2000);
    if (!lockFile.tryLock(100)) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
        QMessageBox::information(nullptr, QStringLiteral("NetDiagnostics Simulator"),
            QStringLiteral("Simulator is already running."));
#endif
        return 0;
    }
    app.setApplicationDisplayName("NetDiagnostics Simulator");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("robinhoo1973");
    app.setWindowIcon(QIcon(":/icons/app-icon.svg"));

    AppState appState;

    QQmlApplicationEngine engine;

    // ── Theme injected directly from C++ ──────────────────────────────
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
    theme["monoFont"]     = QStringLiteral("JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei");
    engine.rootContext()->setContextProperty("Theme", theme);

    engine.rootContext()->setContextProperty("appState", &appState);

    const QUrl url("qrc:/qml/screens/SimulatorScreen.qml");

    // Headless auto-run
    if (qEnvironmentVariableIntValue("ND_AUTORUN")) {
        QTimer::singleShot(5000, &app, [&appState]() {
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
        qCritical("Failed to load SimulatorScreen");
        return -1;
    }

    for (auto* obj : engine.rootObjects()) {
        auto* win = qobject_cast<QQuickWindow*>(obj);
        if (win) {
            win->show();
            break;
        }
    }

    return app.exec();
}
