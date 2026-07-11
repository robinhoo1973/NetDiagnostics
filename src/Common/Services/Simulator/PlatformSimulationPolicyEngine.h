// =============================================================================
// PlatformSimulationPolicyEngine.h — Phase 2: Policy evaluation engine.
//
// Evaluates whether a diagnostic test should run under the current simulation
// configuration.  Distinguishes three skip categories:
//   1. "simulatedPlatform"  — OS/device policy says skip
//   2. "hostPlatform"       — actual runtime platform cannot execute
//   3. "userConfig"         — user disabled the test in Configuration
//
// Wire into AppState via setSkipRules() so the existing bridge continues to
// work, but the UI can query richer status via this engine.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include "TestPolicy.h"

class PlatformSimulationPolicyEngine : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString     activePlatform READ activePlatform NOTIFY platformChanged)
    Q_PROPERTY(int         skipCount      READ skipCount      NOTIFY rulesChanged)
    Q_PROPERTY(QVariantList rules         READ rules          NOTIFY rulesChanged)

public:
    explicit PlatformSimulationPolicyEngine(QObject* parent = nullptr)
        : QObject(parent) {}

    QString activePlatform() const { return m_activePlatform; }
    int skipCount() const { return m_skipReasons.size(); }
    QVariantList rules() const { return m_ruleList; }

    // ── Set the simulated platform and load its policy ──────────────────
    Q_INVOKABLE void setActivePlatform(const QString& platform,
                                        const TestPolicy* policy = nullptr) {
        m_activePlatform = platform;
        m_skipReasons.clear();
        m_ruleList.clear();
        if (policy) {
            for (const auto& r : policy->skipRules) {
                m_skipReasons[r.diagId] = SkipInfo{
                    r.reason, SkipCategory::SimulatedPlatform
                };
                QVariantMap m;
                m["diagId"]   = r.diagId;
                m["testName"] = r.testName;
                m["reason"]   = r.reason;
                m["category"] = "simulatedPlatform";
                m_ruleList.append(m);
            }
        }
        emit platformChanged();
        emit rulesChanged();
    }

    // ── Evaluate a single test ─────────────────────────────────────────
    enum class SkipCategory { None, SimulatedPlatform, HostPlatform, UserConfig };

    struct SkipInfo {
        QString      reason;
        SkipCategory category = SkipCategory::None;
    };

    Q_INVOKABLE QVariantMap evaluateTest(int diagId, bool hostCanRun = true,
                                          bool userEnabled = true) {
        QVariantMap result;
        result["diagId"]    = diagId;
        result["shouldRun"] = true;
        result["skipReason"] = "";
        result["skipCategory"] = "none";

        // 1. Simulated platform policy (highest priority per spec §七)
        auto it = m_skipReasons.find(diagId);
        if (it != m_skipReasons.end()) {
            result["shouldRun"]    = false;
            result["skipReason"]   = it.value().reason;
            result["skipCategory"] = "simulatedPlatform";
            return result;
        }

        // 2. Host platform capability
        if (!hostCanRun) {
            result["shouldRun"]    = false;
            result["skipReason"]   = "Host platform does not support this test";
            result["skipCategory"] = "hostPlatform";
            return result;
        }

        // 3. User configuration
        if (!userEnabled) {
            result["shouldRun"]    = false;
            result["skipReason"]   = "Disabled in Configuration";
            result["skipCategory"] = "userConfig";
            return result;
        }

        return result;
    }

    // ── Generate skip rules for AppState bridge (backward compat) ──────
    Q_INVOKABLE QVariantList toAppStateSkipRules() const {
        return m_ruleList;
    }

    // ── Check if a specific test is skipped by simulated platform ──────
    Q_INVOKABLE bool isSkippedByPlatform(int diagId) const {
        return m_skipReasons.contains(diagId);
    }

    Q_INVOKABLE QString skipReason(int diagId) const {
        auto it = m_skipReasons.find(diagId);
        return it != m_skipReasons.end() ? it.value().reason : QString();
    }

signals:
    void platformChanged();
    void rulesChanged();

private:
    QString m_activePlatform;
    QHash<int, SkipInfo> m_skipReasons;
    QVariantList m_ruleList;
};