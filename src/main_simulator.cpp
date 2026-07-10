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
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QMessageBox>
#endif
#include <QStandardPaths>
#include "app/AppState.h"
#include "simulator/SimulatorConfig.h"
#include "simulator/QtConfigAdapter.h"
#include "simulator/ScreenshotService.h"
#include "simulator/MatrixOrchestrator.h"

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
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
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
#elif defined(__linux__)
    qtConfig.load(QStringLiteral(":/config/linux.conf"));
#elif defined(__APPLE__)
    qtConfig.load(QStringLiteral(":/config/macos.conf"));
#else
    qtConfig.load(QStringLiteral(":/config/linux.conf"));
#endif

    ScreenshotService screenshotSvc;
    screenshotSvc.setOutputDir(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QStringLiteral("/NetDiagnostics_Evidence"));

    MatrixOrchestrator matrixOrch;

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
    // QtWebView availability — avoid QML import crash on platforms
    // without the WebView module (e.g., static MSYS2 builds).
#ifdef HAS_QTWEBVIEW
    engine.rootContext()->setContextProperty("hasWebView", true);
#else
    engine.rootContext()->setContextProperty("hasWebView", false);
#endif
    engine.rootContext()->setContextProperty("simConfig", &simConfig);
    engine.rootContext()->setContextProperty("qtConfig", &qtConfig);
    engine.rootContext()->setContextProperty("screenshotSvc", &screenshotSvc);
    engine.rootContext()->setContextProperty("matrixOrch", &matrixOrch);

    // ══════════════════════════════════════════════════════════════════════
    const QUrl url("qrc:/qml/screens/SimulatorScreen.qml");

    if (qEnvironmentVariableIntValue("ND_AUTORUN")) {
        QTimer::singleShot(5000, &app, [&appState]() {
            appState.setTarget(QStringLiteral("localhost"));
            appState.runDiagnostics();
        });
    }
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [url](const QUrl &objUrl) {
            if (url == objUrl) QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);

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
