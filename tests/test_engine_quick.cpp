// Quick verification that native plugin + engine integrate correctly.
// Build: cd build && cmake .. -DBUILD_TESTS=ON && cmake --build . && ctest -V
#include <QCoreApplication>
#include <QDebug>
#include "app/NativeService.h"
#include "engine/diagnostic/DiagnosticEngine.h"
#include "models/DiagnosticResult.h"
#include "util/Logger.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    Logger::instance().info("Test: Starting engine smoke test");

    // 1. Verify native plugin initializes
    auto& ns = NativeService::instance();
    bool ok = ns.initialize();
    qDebug() << "Native plugin initialized:" << ok << "version:" << ns.version();

    // 2. Create DiagnosticEngine
    DiagnosticEngine engine;

    // 3. Run a native test (G1 NetworkAdapters)
    qDebug() << "Running G1 NetworkAdapters...";
    auto future = engine.runDiag(DiagId::G1NetworkAdapters);
    future.waitForFinished();
    auto result = future.result();
    qDebug() << "  id:" << static_cast<int>(result.id)
             << "status:" << static_cast<int>(result.status)
             << "summary:" << result.summary
             << "durationMs:" << result.durationMs;

    // 4. Run a non-native test (G5 URL parsing)
    qDebug() << "Running G5 URL Parsing...";
    auto future2 = engine.runDiag(DiagId::G5UrlParsing, "https://example.com");
    future2.waitForFinished();
    auto result2 = future2.result();
    qDebug() << "  id:" << static_cast<int>(result2.id)
             << "status:" << static_cast<int>(result2.status)
             << "summary:" << result2.summary;

    ns.shutdown();
    Logger::instance().info("Test: Complete");
    return 0;
}