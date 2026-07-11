// Engine smoke test — verifies DiagnosticResult format and task lifecycle.
// Build: cmake .. -DBUILD_TESTS=ON && cmake --build . && ctest -V
#include <QCoreApplication>
#include <QDebug>
#include <QTest>
#include "models/DiagnosticResult.h"
#include "util/Logger.h"

class EngineSmokeTest : public QObject {
    Q_OBJECT

private slots:
    void testDiagnosticResultFactories() {
        // Verify skipped() creates correct result
        auto skipped = DiagnosticResult::skipped(DiagId::G1NetworkAdapters,
            "Platform not supported");
        QCOMPARE(skipped.id, DiagId::G1NetworkAdapters);
        QCOMPARE(skipped.status, DiagStatus::Skipped);
        QVERIFY(!skipped.summary.isEmpty());
        QVERIFY(!skipped.details.isEmpty());  // 5WHY: was empty — fixed in R3

        // Verify error() creates correct result with details populated
        auto error = DiagnosticResult::error(DiagId::G1NicAdvanced,
            "Test error message");
        QCOMPARE(error.id, DiagId::G1NicAdvanced);
        QCOMPARE(error.status, DiagStatus::Error);
        QCOMPARE(error.summary, QStringLiteral("Test error message"));
        QVERIFY(!error.details.isEmpty());  // 5WHY: details now populated

        // Verify timeout() creates correct result
        auto timeout = DiagnosticResult::timeout(DiagId::G4Ping, 30000);
        QCOMPARE(timeout.id, DiagId::G4Ping);
        QCOMPARE(timeout.group, DiagGroup::G4);  // derived from DiagId
        QCOMPARE(timeout.status, DiagStatus::Error);
        QVERIFY(timeout.summary.contains("30s"));
        QVERIFY(!timeout.details.isEmpty());  // 5WHY: was empty — fixed in R3
        QVERIFY(!timeout.errorOutput.isEmpty());
    }

    void testDiagIdMapping() {
        // Verify DiagId → group mapping is consistent
        QCOMPARE(diagGroup(DiagId::G1NetworkAdapters), DiagGroup::G1);
        QCOMPARE(diagGroup(DiagId::G2TcpSettings), DiagGroup::G2);
        QCOMPARE(diagGroup(DiagId::G3DnsPollution), DiagGroup::G3);
        QCOMPARE(diagGroup(DiagId::G4Ping), DiagGroup::G4);
        QCOMPARE(diagGroup(DiagId::G5UrlParsing), DiagGroup::G5);

        // Verify all DiagIds have valid display names (no "Unknown" fallback)
        for (auto id : allDiagIds()) {
            auto label = diagIdLabelKey(id);
            QVERIFY2(!label.isEmpty(),
                QString("DiagId %1 has empty label key").arg(static_cast<int>(id)).toUtf8());
        }
    }

    void testDiagnosticResultConvenience() {
        DiagnosticResult r;
        r.status = DiagStatus::Pass;
        QVERIFY(r.isPass());
        QVERIFY(!r.isFail());
        QVERIFY(r.wasExecuted());  // 5WHY: deprecated isDone() kept for compat
        QVERIFY(r.isDone());

        r.status = DiagStatus::Skipped;
        QVERIFY(r.isSkipped());
        QVERIFY(!r.wasExecuted());  // Skipped = not executed
    }
};

QTEST_MAIN(EngineSmokeTest)
#include "test_engine_quick.moc"
