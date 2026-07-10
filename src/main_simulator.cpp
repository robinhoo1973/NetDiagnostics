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
#include <QDir>
#include <QFile>
#include <QImage>
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QMessageBox>
#endif
#include <QStandardPaths>
#include "app/AppState.h"

// ── Mockup screenshot mode ──────────────────────────────────────────────
// ND_MOCKUP=1  → enable headless screenshot capture
// ND_MOCKUP_DEVICE=ios-iphone15pm  → device ID from SimulatorScreen.devices
// ND_MOCKUP_OUTPUT=mockups/ios/iphone15pm/dashboard.png  → output path
static bool s_mockupMode = false;
static QString s_mockupDevice;
static QString s_mockupOutput;

int main(int argc, char *argv[])
{
    qputenv("QSG_RENDER_LOOP", "basic");
    QGuiApplication app(argc, argv);
    app.setApplicationName("NetDiagnostics Simulator");

    // ── Single instance via lock file ────────────────────────────────────
    const QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/netdiagnostic-sim.lock");
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
    app.setApplicationVersion(QStringLiteral(PROJECT_VERSION));
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
    theme["monoFont"]     = QStringLiteral("JetBrains Mono");
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

    // ── Mockup screenshot mode ────────────────────────────────────────
    s_mockupMode   = qEnvironmentVariableIntValue("ND_MOCKUP");
    s_mockupDevice = qEnvironmentVariable("ND_MOCKUP_DEVICE");
    s_mockupOutput = qEnvironmentVariable("ND_MOCKUP_OUTPUT");

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical("Failed to load SimulatorScreen");
        return -1;
    }

    QQuickWindow* win = nullptr;
    for (auto* obj : engine.rootObjects()) {
        win = qobject_cast<QQuickWindow*>(obj);
        if (win) break;
    }
    if (!win) {
        qCritical("No QQuickWindow in rootObjects");
        return -1;
    }

    if (s_mockupMode) {
        // ── Mockup screenshot mode ───────────────────────────────────
        // 5WHY best practice: show window → render first frame →
        // capture via frameSwapped signal → save → exit.
        // This guarantees the frame buffer is populated before grabWindow().
        // Switch to requested device, if available in the QML model
        QObject* root = win->contentItem();
        if (root) {
            QVariant v = root->property("devices");
            if (v.isValid() && v.canConvert<QVariantList>()) {
                QVariantList devices = v.value<QVariantList>();
                for (int i = 0; i < devices.size(); ++i) {
                    if (devices[i].toMap().value("id").toString() == s_mockupDevice) {
                        root->setProperty("currentDevice", i);
                        break;
                    }
                }
                QCoreApplication::processEvents();
            }
        }

        win->show();

        // Wait for QML to render, then capture and exit.
        // The window must be visible for the scene graph to render.
        const QString outPath = s_mockupOutput;
        QTimer::singleShot(3000, win, [win, outPath]() {
            if (outPath.isEmpty()) {
                qCritical("Mockup FAILED: empty output path");
                QCoreApplication::exit(1);
                return;
            }
            QImage img = win->grabWindow();
            if (img.isNull()) {
                qCritical("Mockup FAILED: grabWindow null");
                QCoreApplication::exit(1);
                return;
            }
            QDir().mkpath(QFileInfo(outPath).absolutePath());
            img.save(outPath);
            qInfo("Mockup: %s (%dx%d)", qPrintable(outPath), img.width(), img.height());
            QCoreApplication::exit(0);
        });
    } else {
        win->show();
    }

    return app.exec();
}
