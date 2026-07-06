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
    app.setApplicationVersion("0.0.1");
    app.setOrganizationName("robinhoo1973");
    app.setWindowIcon(QIcon(":/icons/app-icon.svg"));

    AppState appState;

    QQmlApplicationEngine engine;

    // ── ThemeEngine (QML singleton) replaces old C++ Theme QVariantMap ──
    // Theme switching (System/Light/Dark) is handled by
    // resources/qml/theme/ThemeEngine.qml — see main.cpp for details.

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
