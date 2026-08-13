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
#include "App/AppState.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QEventLoop>
#include <QCollator>
#include <QNetworkProxyFactory>
#include <QFile>

#include <cstdio>

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
        DiagnosticSuite suite(g);
        QVector<DiagId> ids;
        for (DiagId id : allDiagIds())
            if (diagGroup(id) == g && isSchedulable(id))
                ids.append(id);
        suite.setDiagIds(ids);

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

        suite.run(QStringLiteral("example.com"), QStringLiteral("https"));
        if (!done) loop.exec();   // wait for completion (async probes)
    }

    std::printf("selftest: %d results, verify=%s, contract=%s\n", total,
                AdapterRegistry::verifyAllDiagIds() ? "PASS" : "GAPS",
                contractViolations == 0 ? "PASS" : "VIOLATIONS");
    return (total > 0 && contractViolations == 0) ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("NetDiagnostic"));
    // QSettings 持久化需 organizationName；缺省时 Windows 写入未知组织键
    QCoreApplication::setOrganizationName(QStringLiteral("NetDiagnostics"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1"));
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

    // Windows GUI 子系统无控制台——QML 加载错误等 qWarning 默认不可见，
    // 统一转发到 stderr（重定向可捕获；日志即诊断）。
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
        std::fprintf(stderr, "%s\n", qPrintable(msg));
    });

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NetDiagnostics — network diagnostic suite"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption selftestOption(QStringLiteral("selftest"),
        QStringLiteral("Run the G1-G5 headless self-test and exit"));
    parser.addOption(selftestOption);
    parser.process(app);

    // ── Contract/execution layer bootstrap (DIAG-2/A1, NEW-16) ────────────
    registerAllAdapters();
    enforceStartupInvariant(AdapterRegistry::verifyAllDiagIds());

    if (parser.isSet(selftestOption))
        return runSelftest();

    // ── UI 层（P0：AppState 桥接 + DiagnosticScreen）───────────────
    AppState appState;
    qmlRegisterSingletonInstance("NetDiagnostics.App", 1, 0, "AppState", &appState);

    QQmlApplicationEngine engine;
    // qrc 目录导入在 Qt6 不可用：统一走 /qt/qml 导入路径下的 qmldir 模块
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

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
