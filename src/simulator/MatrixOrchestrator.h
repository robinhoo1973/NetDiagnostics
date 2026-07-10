// =============================================================================
// MatrixOrchestrator.h — Phase 4: Full Matrix Test execution engine.
//
// Generates the Cartesian product of OS × Device × Protocol × Target × Tests,
// executes each combination through AppState, and collects results/evidence.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDateTime>

class MatrixOrchestrator : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList matrix   READ matrix   NOTIFY matrixChanged)
    Q_PROPERTY(int currentStep       READ currentStep NOTIFY progressChanged)
    Q_PROPERTY(int totalSteps        READ totalSteps  NOTIFY progressChanged)
    Q_PROPERTY(QString currentOS     READ currentOS   NOTIFY progressChanged)
    Q_PROPERTY(QString currentDevice READ currentDevice NOTIFY progressChanged)
    Q_PROPERTY(bool   running        READ running       NOTIFY runningChanged)

public:
    explicit MatrixOrchestrator(QObject* parent = nullptr) : QObject(parent) {}

    QVariantList matrix() const { return m_matrix; }
    int currentStep() const { return m_currentStep; }
    int totalSteps() const { return m_matrix.size(); }
    QString currentOS() const { return m_currentOS; }
    QString currentDevice() const { return m_currentDevice; }
    bool running() const { return m_running; }

    // ── Generate matrix from profile sets ──────────────────────────────
    Q_INVOKABLE void generate(const QVariantList& osList,
                               const QVariantList& deviceList,
                               const QVariantList& targetList,
                               const QVariantList& testList) {
        m_matrix.clear();
        for (const auto& osV : osList) {
            QVariantMap osM = osV.toMap();
            QString osId = osM.value("id").toString();
            for (const auto& devV : deviceList) {
                QVariantMap devM = devV.toMap();
                if (devM.value("os").toString() != osId) continue; // only matching OS devices
                for (const auto& tgtV : targetList) {
                    QVariantMap tgtM = tgtV.toMap();
                    for (const auto& testV : testList) {
                        QVariantMap entry;
                        entry["os"]       = osId;
                        entry["device"]   = devM.value("id").toString();
                        entry["target"]   = tgtM.value("targetUrl").toString();
                        entry["port"]     = tgtM.value("port").toInt();
                        entry["protocol"] = tgtM.value("protocolSchema").toString();
                        entry["test"]     = testV.toString();
                        m_matrix.append(entry);
                    }
                }
            }
        }
        m_currentStep = 0;
        emit matrixChanged();
        m_running = false;
        emit runningChanged();
    }

    // ── Run next step ──────────────────────────────────────────────────
    Q_INVOKABLE QVariantMap nextStep() {
        if (m_currentStep >= m_matrix.size()) return {};
        QVariantMap step = m_matrix[m_currentStep].toMap();
        m_currentOS     = step.value("os").toString();
        m_currentDevice = step.value("device").toString();
        m_currentStep++;
        emit progressChanged();
        return step;
    }

    Q_INVOKABLE void reset() {
        m_currentStep = 0;
        m_running = false;
        m_results.clear();
        emit progressChanged();
        emit runningChanged();
    }

    Q_INVOKABLE void start()  { m_running = true;  emit runningChanged(); }
    Q_INVOKABLE void stop()   { m_running = false; emit runningChanged(); }

    // ── Results collection ──────────────────────────────────────────────
    Q_INVOKABLE void addResult(const QVariantMap& r) { m_results.append(r); }

    Q_INVOKABLE QString exportReport(const QString& format = QStringLiteral("json")) {
        QJsonObject report;
        report["generated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        report["totalSteps"] = m_matrix.size();
        report["completedSteps"] = m_results.size();

        QJsonArray resultsArr;
        for (const auto& r : m_results) {
            resultsArr.append(QJsonObject::fromVariantMap(r.toMap()));
        }
        report["results"] = resultsArr;

        QString path = QStringLiteral("matrix_report_%1.%2")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"), format);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
            f.close();
        }
        return QFileInfo(path).absoluteFilePath();
    }

signals:
    void matrixChanged();
    void progressChanged();
    void runningChanged();

private:
    QVariantList m_matrix;
    QVariantList m_results;
    int          m_currentStep = 0;
    bool         m_running = false;
    QString      m_currentOS;
    QString      m_currentDevice;
};

#include "moc_MatrixOrchestrator.cpp"
