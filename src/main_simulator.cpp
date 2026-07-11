// =============================================================================
// main_simulator.cpp — Simulator entry point (Phase 1-4 modules).
//
// Architecture per build/simulator.md:
//   SimulatorConfig → profile loading (devices, OS, test_policy)
//   QtConfigAdapter → production qtquickcontrols2.conf adapter
//   ScreenshotService → viewport-cropped capture + evidence log
//   MatrixOrchestrator → full-matrix test execution (Phase 4)
//   AppState → shared diagnostic engine (skip-policy bridge: Phase 2)
// =============================================================================
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QVariantMap>
#include <QTimer>
#include <QQuickWindow>
#include <QQuickItem>
#include <QIcon>
#include <QLockFile>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QFileInfo>
#if(!defined(PLATFORM_IOS)&&!defined(PLATFORM_ANDROID))
#include <QMessageBox>
#endif
#include <QStandardPaths>
#include "app/AppState.h"
#include "Dashboard/Controller/DashboardController.h"
#include "Diagnostics/Controller/DiagnosticsController.h"
#include "Configuration/Controller/ConfigurationController.h"
#include "Report/Controller/ReportController.h"
#include "Settings/Controller/SettingsController.h"
#include "Common/Utils/StartupLog.h"
#include "Common/Utils/CrashHandler.h"
#include "Common/Services/Simulator/SimulatorConfig.h"
#include "Common/Services/Simulator/QtConfigAdapter.h"
#include "Common/Services/Simulator/ScreenshotService.h"
#include "Common/Services/Simulator/MatrixOrchestrator.h"
#include "Common/Services/Simulator/PlatformSimulationPolicyEngine.h"
#include "Common/Services/Simulator/ProtocolRegistry.h"

int main(int argc, char *argv[])
{
    qputenv("QSG_RENDER_LOOP", "basic");
    QGuiApplication app(argc, argv);
    app.setApplicationName("NetDiagnostics Simulator");

    // ── Single instance via lock file ────────────────────────────────────
    const QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                           + QStringLiteral("/netdiagnostic-sim.lock");
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(2000);
    if (!lockFile.tryLock(100)) {
#if(!defined(PLATFORM_IOS)&&!defined(PLATFORM_ANDROID))
        QMessageBox::information(nullptr, QStringLiteral("NetDiagnostics Simulator"),
            QStringLiteral("Simulator is already running."));
#endif
        return 0;
    }
    app.setApplicationDisplayName("NetDiagnostics Simulator");
    app.setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    app.setOrganizationName("robinhoo1973");
    app.setWindowIcon(QIcon(":/icons/netanalysis.ico"));

    // ══════════════════════════════════════════════════════════════════════
    // Core modules
    AppState appState;

    SimulatorConfig simConfig;
    QString profilesDir = QDir::currentPath() + QStringLiteral("/profiles");
    if (!QDir(profilesDir).exists())
        profilesDir = QCoreApplication::applicationDirPath() + QStringLiteral("/profiles");
    if (!QDir(profilesDir).exists())
        profilesDir = QCoreApplication::applicationDirPath() + QStringLiteral("/../profiles");
    simConfig.loadAll(profilesDir);

    QtConfigAdapter qtConfig;
#if defined(_WIN32)
    qtConfig.load(QStringLiteral(":/config/windows.conf"));
#else
#if defined(__linux__)
    qtConfig.load(QStringLiteral(":/config/linux.conf"));
#else
#if defined(__APPLE__)
    qtConfig.load(QStringLiteral(":/config/macos.conf"));
#else
    qtConfig.load(QStringLiteral(":/config/linux.conf"));
#endif
#endif
#endif

    ScreenshotService screenshotSvc;
    screenshotSvc.setOutputDir(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QStringLiteral("/NetDiagnostics_Evidence"));

    MatrixOrchestrator matrixOrch;
    PlatformSimulationPolicyEngine policyEngine;

    // ══════════════════════════════════════════════════════════════════════
    // QML engine + context properties
    QQmlApplicationEngine engine;

    QVariantMap theme;
    theme["bgDark"]=QStringLiteral("#1E1E2E");theme["bgSidebar"]=QStringLiteral("#252538");
    theme["bgCard"]=QStringLiteral("#16213E");theme["bgInput"]=QStringLiteral("#2A2A4A");
    theme["textPrimary"]=QStringLiteral("#E0E0E0");theme["textSecondary"]=QStringLiteral("#A0A0B8");
    theme["textMuted"]=QStringLiteral("#606080");theme["accent"]=QStringLiteral("#E94560");
    theme["accentBlue"]=QStringLiteral("#0078D4");theme["cyan"]=QStringLiteral("#00BCD4");
    theme["passGreen"]=QStringLiteral("#4ADE80");theme["warnYellow"]=QStringLiteral("#FACC15");
    theme["failRed"]=QStringLiteral("#EF4444");theme["skipGray"]=QStringLiteral("#888888");
    theme["infoBlue"]=QStringLiteral("#0078D4");theme["borderCard"]=QStringLiteral("#3A3A5A");
    theme["borderSubtle"]=QStringLiteral("#2A2A4A");theme["borderFocused"]=QStringLiteral("#0078D4");
    theme["radiusCard"]=12.0;theme["radiusButton"]=8.0;theme["radiusSmall"]=6.0;
    theme["sidebarWidth"]=260.0;theme["monoFont"]=QStringLiteral("JetBrains Mono");
    engine.rootContext()->setContextProperty("Theme", theme);

    engine.rootContext()->setContextProperty("appState", &appState);
    engine.rootContext()->setContextProperty("dashboardCtrl", QVariant::fromValue(static_cast<QObject*>(appState.dashboardController())));
    engine.rootContext()->setContextProperty("diagCtrl", QVariant::fromValue(static_cast<QObject*>(appState.diagnosticsController())));
    engine.rootContext()->setContextProperty("configCtrl", QVariant::fromValue(static_cast<QObject*>(appState.configurationController())));
    engine.rootContext()->setContextProperty("reportCtrl", QVariant::fromValue(static_cast<QObject*>(appState.reportController())));
    engine.rootContext()->setContextProperty("settingsCtrl", QVariant::fromValue(static_cast<QObject*>(appState.settingsController())));
    // QtWebView availability — avoid QML import crash on platforms
    // without the WebView module (e.g., static MSYS2 builds).
#if defined(HAS_QTWEBVIEW)
    engine.rootContext()->setContextProperty("hasWebView", true);
#else
    engine.rootContext()->setContextProperty("hasWebView", false);
#endif
#if defined(HAS_QTPDF)
    engine.rootContext()->setContextProperty("hasQtPdf", true);
#else
    engine.rootContext()->setContextProperty("hasQtPdf", false);
#endif
#if defined(__APPLE__) || defined(PLATFORM_ANDROID)
    engine.rootContext()->setContextProperty("hasNativePdf", true);
#else
    engine.rootContext()->setContextProperty("hasNativePdf", false);
#endif
    engine.rootContext()->setContextProperty("simConfig", &simConfig);
    engine.rootContext()->setContextProperty("qtConfig", &qtConfig);
    engine.rootContext()->setContextProperty("screenshotSvc", &screenshotSvc);
    engine.rootContext()->setContextProperty("matrixOrch", &matrixOrch);
    engine.rootContext()->setContextProperty("policyEngine", &policyEngine);
    engine.rootContext()->setContextProperty("protoReg", &ProtocolRegistry::instance());

    // ══════════════════════════════════════════════════════════════════════
    const QUrl url("qrc:/qml/screens/SimulatorScreen.qml");

    STARTUP_SEPARATOR();
    STARTUP_LOG("NetDiagnostics Simulator starting, Qt %s, edition=%s",
                qVersion(), APP_EDITION);

    if (qEnvironmentVariableIntValue("ND_AUTORUN")) {
        QTimer::singleShot(5000, &app, [&appState]() {
            appState.setTarget(QStringLiteral("localhost"));
            appState.runDiagnostics();
        });
    }
    // Capture QML warnings/errors to the startup log
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
        &engine, [](const QList<QQmlError>& warnings) {
            for (const auto& w : warnings)
                STARTUP_LOG("Simulator QML WARNING: %s", w.toString().toUtf8().constData());
        });

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [url](const QUrl &objUrl) {
            STARTUP_LOG("Simulator QML FATAL: object creation failed for %s", objUrl.toString().toUtf8().constData());
            if (url == objUrl) QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);

    STARTUP_LOG("Simulator calling engine.load()...");
    engine.load(url);
    STARTUP_LOG("Simulator engine.load() returned. rootObjects=%d", engine.rootObjects().size());

    if (engine.rootObjects().isEmpty()) {
        // 5WHY: Simulator crash was silent on desktop — no diagnostic visible
        // to the user. Match the pattern from main.cpp to show a message box
        // so the user can report the root cause instead of seeing flash-and-quit.
        STARTUP_LOG("FATAL: Simulator QML engine failed to load %s — no root objects", "qrc:/qml/screens/SimulatorScreen.qml");
        qCritical("Failed to load SimulatorScreen");
#if(!defined(PLATFORM_IOS)&&!defined(PLATFORM_ANDROID))
        QMessageBox::critical(nullptr, QStringLiteral("NetDiagnostics Simulator — Startup Error"),
            QStringLiteral("Failed to load the Simulator QML UI.\n\n"
            "This usually means a required Qt module is missing from your installation.\n"
            "Check the startup log at: %1\n\n"
            "Common causes:\n"
            "• Qt Quick module not installed\n"
            "• Static build missing QML plugins\n"
            "• Corrupted resources.qrc file")
            .arg(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                 .filePath("NetDiagnostics_startup.log")));
#endif
        return -1;
    }

    QQuickWindow* win = nullptr;
    for (auto* obj : engine.rootObjects()) {
        win = qobject_cast<QQuickWindow*>(obj);
        if (win) break;
    }
    if (!win) { qCritical("No QQuickWindow in rootObjects"); return -1; }

    // Bind ScreenshotService to DeviceViewport after scene graph is ready
    QTimer::singleShot(500, win, [win, &screenshotSvc]() {
        QQuickItem* vp = win->findChild<QQuickItem*>(QStringLiteral("deviceViewport"));
        if (vp) screenshotSvc.setViewport(vp);
    });

    // ── Mockup screenshot mode ─────────────────────────────────────────
    bool mockupMode = qEnvironmentVariableIntValue("ND_MOCKUP");
    if (mockupMode) {
        QString mockupDevice = qEnvironmentVariable("ND_MOCKUP_DEVICE");
        QString mockupOutput = qEnvironmentVariable("ND_MOCKUP_OUTPUT");
        QObject* root = win->contentItem();
        if (root && !mockupDevice.isEmpty()) {
            QVariant v = root->property("devices");
            if (v.isValid() && v.canConvert<QVariantList>()) {
                QVariantList devices = v.value<QVariantList>();
                for (int i = 0; i < devices.size(); ++i) {
                    if (devices[i].toMap().value("id").toString() == mockupDevice) {
                        root->setProperty("currentDevice", i); break;
                    }
                }
                QCoreApplication::processEvents();
            }
        }
        win->show();
        const QString outPath = mockupOutput;
        QTimer::singleShot(3000, win, [win, outPath]() {
            if (outPath.isEmpty()) { QCoreApplication::exit(1); return; }
            QImage img = win->grabWindow();
            if (img.isNull()) { QCoreApplication::exit(1); return; }
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
