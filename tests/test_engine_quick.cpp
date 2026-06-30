// Engine smoke test — verifies TaskFactory dispatch and result format.
// Build: cd build && cmake .. -DBUILD_TESTS=ON && cmake --build . && ctest -V
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include "engine/task/TaskFactory.h"
#include "models/DiagnosticResult.h"
#include "util/Logger.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    Logger::instance().info("Test: Starting engine smoke test");

    // 1. Create a task via TaskFactory
    qDebug() << "Running G1 NetworkAdapters...";
    auto task = TaskFactory::createTask(DiagId::G1NetworkAdapters);
    if (!task) {
        qCritical() << "FAIL: TaskFactory returned null for G1NetworkAdapters";
        return 1;
    }

    // 2. Run synchronously via QtConcurrent (simulates the AppState dispatch)
    QElapsedTimer t; t.start();
    auto future = QtConcurrent::run([tptr = task.get()]() { return tptr->run(); });
    future.waitForFinished();
    auto result = future.result();
    qDebug() << "  id:" << static_cast<int>(result.id)
             << "status:" << static_cast<int>(result.status)
             << "summary:" << result.summary
             << "durationMs:" << result.durationMs;
    t.elapsed();

    // 3. Verify result
    if (result.id != DiagId::G1NetworkAdapters) {
        qCritical() << "FAIL: wrong DiagId";
        return 1;
    }
    if (result.status == DiagStatus::Error) {
        qCritical() << "FAIL: diagnostic returned error";
        return 1;
    }

    // 4. Test G4 DNS resolution
    qDebug() << "Running G4 DnsResolution...";
    auto dnsTask = TaskFactory::createTask(DiagId::G4DnsResolution, "localhost");
    if (!dnsTask) {
        qCritical() << "FAIL: TaskFactory returned null for G4DnsResolution";
        return 1;
    }
    auto dnsFuture = QtConcurrent::run([tptr = dnsTask.get()]() { return tptr->run(); });
    dnsFuture.waitForFinished();
    auto dnsResult = dnsFuture.result();
    qDebug() << "  id:" << static_cast<int>(dnsResult.id)
             << "status:" << static_cast<int>(dnsResult.status)
             << "summary:" << dnsResult.summary;

    if (dnsResult.id != DiagId::G4DnsResolution) {
        qCritical() << "FAIL: wrong DiagId for DNS test";
        return 1;
    }

    Logger::instance().info("Test: engine smoke test PASSED");
    qDebug() << "PASS";
    return 0;
}
