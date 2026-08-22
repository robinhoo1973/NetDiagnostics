// =============================================================================
// main.cpp — NetDiagnostics entry point (rebuilt per refactor docs)
//
// Wiring order (DIAG-2/A1 + NEW-16):
//   registerAllAdapters() → verifyAllDiagIds() → fail-fast(debug/CI)/fail-safe(release)
//   → selftest (--selftest) or QML application.
// =============================================================================
#include "Diagnostics/Model/Adapters.h"
#include "Common/Services/PlatformAdapter.h"   // AdapterRegistry（§6）
#include "Diagnostics/Model/DiagnosticSuite.h"
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"
#include "app/AppState.h"
#include "Common/Utils/CrashHandler.h"
#include "Common/Utils/StartupLog.h"
#include "Settings/Model/PremiumStore.h"
#include "Common/Services/IconProvider.h"   // 图标管线 v4：image://icon 运行时着色
#if defined(PLATFORM_IOS)
#include "Diagnostics/Model/G1/Platform/IOS/IosNetworkInfo.h"   // iosRequestWiFiAuthorization
#endif
#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
#include "Common/Platform/NativePdfDocument.h"
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QEventLoop>
#include <QCollator>
#include <QNetworkProxyFactory>
#include <QFile>
#include <QDir>
#include <QLockFile>

#include <csignal>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#endif

// 5WHY (2026-08-22 WifiWave SVG 复绘): 动画层以 data:image/svg+xml;base64
// 数据 URI 重绘母版弧线——QML Image 解码 SVG 走 qsvg imageformat 插件。
// 静态 Qt（iOS/Android/Windows-MSYS2）下插件不会自动注册，必须显式
// Q_IMPORT_PLUGIN(QSvgPlugin)（Qt 官方静态构建文档惯例）；Qt6::Svg 由
// cmake/netdiag-target.cmake 在 Qt6Svg_FOUND 时统一链接（IconProvider
// 的 QSvgRenderer 同源依赖，全平台已具备）。动态 Qt 自动发现插件，
// 此宏为 no-op（QT_STATIC 未定义）。
#if defined(QT_STATIC)
#include <QtPlugin>
Q_IMPORT_PLUGIN(QSvgPlugin)
#endif

namespace {

// NEW-16: verify failure policy — fail-fast on debug/CI, hide+continue on release.
void enforceStartupInvariant(bool ok) {
#if !defined(ND_RELEASE)
    if (!ok) {
        qFatal("AdapterRegistry::verifyAllDiagIds() failed — platform adapter gap. "
               "Fix the registration before shipping.");
    }
#else
    if (!ok)
        qWarning() << "AdapterRegistry::verifyAllDiagIds() reported gaps; affected "
                      "tests will be hidden on this platform.";
#endif
}

// ── Headless self-test: run ALL five suites and print per-test results
//    (proves the 45-entry registry + capability/scheme/device gating
//    end-to-end without a UI).
// G5 scheme 过滤落库后（diag-g5 §2.21），每个协议检测必须用自身 scheme 调度，
// 否则 select() 返回 nullptr → 检测被隐藏、CI 覆盖丢失。
const char* selftestSchemeFor(DiagId id) {
    switch (id) {
        case DiagId::G5FtpDiagnostics:   return "ftp";
        case DiagId::G5SshDiagnostics:   return "ssh";
        case DiagId::G5EmailDiagnostics: return "smtp";
        case DiagId::G5Telnet:           return "telnet";
        case DiagId::G5Mysql:            return "mysql";
        case DiagId::G5Postgres:         return "postgresql";
        case DiagId::G5Redis:            return "redis";
        case DiagId::G5Mongodb:          return "mongodb";
        case DiagId::G5Ldap:             return "ldap";
        case DiagId::G5Mqtt:             return "mqtt";
        // ServiceBanner = 排除 http/https（diag-g5 §2.21）——用 ftp 触发
        case DiagId::G5ServiceBanner:    return "ftp";
        default:                         return "https";
    }
}

int runSelftest() {
    // GUI 子系统下 stdout 全缓冲：崩溃时日志会丢。selftest 改为行缓冲，
    // 保证每条结果实时落盘（也便于定位崩溃点）。
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    int total = 0;
    // R5-3（契约自检）：Pass 结果必须携带 meta.keyMetricField 声明的主指标，
    // 否则指标卡/图表拿不到数据——在自检阶段提前暴露探针与契约的漂移。
    int contractViolations = 0;
    const DiagGroup groups[] = { DiagGroup::G1, DiagGroup::G2, DiagGroup::G3, DiagGroup::G4, DiagGroup::G5 };
    for (DiagGroup g : groups) {
        // G1-G4 无 scheme 过滤（通配）→ 单批 https；G5 按检测 scheme 分批。
        QHash<QString, QVector<DiagId>> batches;
        if (g == DiagGroup::G5) {
            for (DiagId id : allDiagIds())
                if (diagGroup(id) == g && isSchedulable(id))
                    batches[QLatin1String(selftestSchemeFor(id))].append(id);
        } else {
            QVector<DiagId> ids;
            for (DiagId id : allDiagIds())
                if (diagGroup(id) == g && isSchedulable(id))
                    ids.append(id);
            batches.insert(QStringLiteral("https"), ids);
        }

        for (auto it = batches.constBegin(); it != batches.constEnd(); ++it) {
            DiagnosticSuite suite(g);
            suite.setDiagIds(it.value());

            QEventLoop loop;
            bool done = false;
            QObject::connect(&suite, &DiagnosticSuite::suiteFinished,
                             [&loop, &done]() { done = true; loop.quit(); });
            QObject::connect(&suite, &DiagnosticSuite::resultReady,
                             [&total, &contractViolations](const DiagnosticResult& r) {
                std::printf("[%s] %-30s -> %s\n",
                            qPrintable(diagGroupLabel(r.group)),
                            qPrintable(r.displayName),
                            qPrintable(r.summary.isEmpty() ? "ok" : r.summary));
                ++total;
                if (r.status != DiagStatus::Pass) return;
                const DetailProfile& d = diagnosticMeta(r.id).detail;
                if (d.keyMetricField && !r.data.contains(QLatin1String(d.keyMetricField))) {
                    ++contractViolations;
                    std::printf("CONTRACT: [%s] Pass without key metric '%s'\n",
                                qPrintable(r.displayName), d.keyMetricField);
                }
            });

            // G5 批次的 target 必须带 scheme 前缀（探针按 target 内 scheme
            // 判定协议），G1-G4 无 scheme 过滤，裸 host 即可。
            const QString target = (g == DiagGroup::G5)
                ? it.key() + QLatin1String("://example.com")
                : QStringLiteral("example.com");
            suite.run(target, it.key());
            if (!done) loop.exec();   // wait for completion (async probes)
        }
    }

    std::printf("selftest: %d results, verify=%s, contract=%s\n", total,
                AdapterRegistry::verifyAllDiagIds() ? "PASS" : "GAPS",
                contractViolations == 0 ? "PASS" : "VIOLATIONS");
    return (total > 0 && contractViolations == 0) ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
    // 5WHY（SIGPIPE 杀进程）：G4/G5 大量裸 socket 对已关闭连接写会触发
    // SIGPIPE 直接终止进程——诊断工具必须忽略。
#if !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif

    // Crash handler 必须在 QGuiApplication 之前安装，捕获 ctor 阶段崩溃。
    CrashHandler::install();
#if defined(ND_DEBUG) || defined(ND_TESTING)
    STARTUP_LOG("main() entered — native entry, pre-QGuiApplication");
    CrashHandler::checkForPreviousCrash();
#endif

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("NetDiagnostic"));
    // QSettings 持久化需 organizationName；缺省时 Windows 写入未知组织键
    QCoreApplication::setOrganizationName(QStringLiteral("NetDiagnostics"));
    // 5WHY (review 2026-08-17): 版本字面量 "0.0.1" 是 Qt6 移植时的占位符，
    // 从未接上 PROJECT_VERSION 编译定义——Settings About、--version 均显示
    // 0.0.1，且覆盖了 macOS/iOS bundle 的正确版本（netdiag-target.cmake 注入）。
    // 单一事实源：CMake 的 PROJECT_VERSION（CI 从 v* tag 派生）。
    QCoreApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    // 预热 QCollator（Qt 惰性初始化与线程池并发首次使用存在竞态，
    // 在主线程先触发一次初始化可消除窗口）——selftest 与 QML 路径共用。
    {
        QCollator warm;
        warm.sortKey(QStringLiteral("warm"));
    }
    // 5WHY (间歇 SIGSEGV @ worker threads)：QAbstractSocket::connectToHost 在
    // 每个线程首次连接时调用 QNetworkProxyFactory::systemProxyForQuery()（Windows
    // 下经 mswsock select/WinHTTP 查系统代理），多线程并发首次查询存在竞态
    // （栈损坏崩溃）。诊断工具本应直连目标，禁用系统代理枚举：
    // ①消除竞态；②探测结果不受本机代理配置干扰。
    QNetworkProxyFactory::setUseSystemConfiguration(false);

    // 5WHY (review 2026-08-17): 此处曾有 qInstallMessageHandler(stderr lambda)
    // ——它静默替换 CrashHandler::install()（line 149）安装的 qtMessageHandler，
    // 使 qFatal 文本（iOS QML 加载失败的根因载体）从此写不进 crash log。
    // CrashHandler::qtMessageHandler 本身已回显 stderr（CrashHandler.h:313/326），
    // 该块纯冗余且有害，已删除。

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NetDiagnostics — network diagnostic suite"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption selftestOption(QStringLiteral("selftest"),
        QStringLiteral("Run the G1-G5 headless self-test and exit"));
    parser.addOption(selftestOption);
#if defined(ND_TESTING)
    QCommandLineOption testOption(QStringLiteral("test"),
        QStringLiteral("Headless CI alias of --selftest"));
    parser.addOption(testOption);
#endif
    parser.process(app);

    // ── Contract/execution layer bootstrap (DIAG-2/A1, NEW-16) ────────────
    registerAllAdapters();
    enforceStartupInvariant(AdapterRegistry::verifyAllDiagIds());

    if (parser.isSet(selftestOption)
#if defined(ND_TESTING)
        || parser.isSet(testOption)
#endif
        )
        return runSelftest();

    // ── 单实例锁：Windows 命名互斥量；Linux/macOS QLockFile ─────────────
#if defined(_WIN32)
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"Global\\NetDiagnostic_SingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }
#else
    QLockFile singleInstanceLock(
        QDir::temp().filePath(QStringLiteral("NetDiagnostics.lock")));
    singleInstanceLock.setStaleLockTime(30000);   // 30s 陈旧判定，防崩溃遗留死锁
    if (!singleInstanceLock.tryLock(100)) return 0;
#endif

    // ── UI 层（P0：AppState 桥接 + DiagnosticScreen）───────────────
    AppState appState;
    qmlRegisterSingletonInstance("NetDiagnostics.App", 1, 0, "AppState", &appState);
    // PremiumStore（Premium 后端恢复）：由 AppState 持有生命周期
    qmlRegisterSingletonInstance("NetDiagnostics.App", 1, 0, "PremiumStore",
                                 appState.premiumStore());
#if defined(PLATFORM_IOS)
    // 5WHY (复核 2026-08-21 用户 "iOS 提示 Location Permission"): v0.0.3
    // main.cpp 启动时请求 CLLocationManager WhenInUse 授权（iosRequestWiFi
    // Authorization）——重构后该调用丢失，授权状态恒 NotDetermined，
    // NEHotspotNetwork 返回空 SSID，WiFi 检测提示 Location Permission。
    // 恢复启动序请求（提示仅出现一次，后续调用为 no-op）。
    iosRequestWiFiAuthorization();
#endif
#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
    // 5WHY (review 2026-08-17): NativePdfDocument 从未注册到 QML 引擎——
    // NativePdfPageView.qml 的 typeof 守卫恒为 false，iOS/macOS 原生 PDF
    // 渲染栈是不可达死代码。
    qmlRegisterType<NativePdfDocument>("NetDiagnostics", 1, 0, "NativePdfDocument");
#endif

    QQmlApplicationEngine engine;
    // qrc 目录导入在 Qt6 不可用：统一走 /qt/qml 导入路径下的 qmldir 模块
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

    // 图标管线 v4：单母版运行时精确着色（image://icon/<hex>/<name>?theme=..&dpr=..）
    // Qt 接管 provider 生命周期；注册须在 engine.load 之前。
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider());

    // 条件预览组件标志（hasWebView/hasQtPdf/hasNativePdf——归档语义恢复）
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
#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
    engine.rootContext()->setContextProperty("hasNativePdf", true);
#else
    engine.rootContext()->setContextProperty("hasNativePdf", false);
#endif

    // 翻译数据：同步 XHR 在 qrc:/ 上被 Qt 6.8 阻断（"Invalid state"），
    // 由 C++ 读取 :/translations.json 以 TJson context property 暴露给 T 代理。
    {
        QFile tf(QStringLiteral(":/translations.json"));
        if (tf.open(QIODevice::ReadOnly))
            engine.rootContext()->setContextProperty(
                QStringLiteral("TJson"),
                QString::fromUtf8(tf.readAll()));
    }

    const QUrl url(QStringLiteral("qrc:/qt/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.load(url);
    return app.exec();
}
