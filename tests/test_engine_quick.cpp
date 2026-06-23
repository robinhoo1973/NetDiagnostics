// Engine smoke test — verifies pure C++ diagnostic dispatch and result format.
// Build: cd build && cmake .. -DBUILD_TESTS=ON && cmake --build . && ctest -V
#include <QCoreApplication>
#include <QDebug>
#include "engine/diagnostic/DiagnosticEngine.h"
#include "models/DiagnosticResult.h"
#include "util/Logger.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    Logger::instance().info("Test: Starting engine smoke test");

    // 1. Create DiagnosticEngine — pure C++ dispatch, no native plugin
    DiagnosticEngine engine;

    // 2. Run a G1 test (Network Adapters)
    qDebug() << "Running G1 NetworkAdapters...";
    auto future = engine.runDiag(DiagId::G1NetworkAdapters);
    future.waitForFinished();
    auto result = future.result();
    qDebug() << "  id:" << static_cast<int>(result.id)
             << "status:" << static_cast<int>(result.status)
             << "summary:" << result.summary
             << "durationMs:" << result.durationMs;

    // 3. Run a G5 test (URL Parsing)
    qDebug() << "Running G5 URL Parsing...";
    auto future2 = engine.runDiag(DiagId::G5UrlParsing, "https://example.com");
    future2.waitForFinished();
    auto result2 = future2.result();
    qDebug() << "  id:" << static_cast<int>(result2.id)
             << "status:" << static_cast<int>(result2.status)
             << "summary:" << result2.summary;

    Logger::instance().info("Test: Complete");
    return 0;
}
