// =============================================================================
// SimulatorConfig.h — Central configuration manager for the simulator.
//
// Loads OS profiles, device profiles, test policies, target profiles, and
// test suites from JSON files under profiles/**.  Exposes them as QVariantList
// for consumption by QML via context properties.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantList>
#include <QVariantMap>
#include <QDir>
#include <QFileInfo>
#include "DeviceProfile.h"
#include "OSProfile.h"
#include "TestPolicy.h"

class SimulatorConfig : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList devices     READ devices     NOTIFY profilesChanged)
    Q_PROPERTY(QVariantList osList      READ osList      NOTIFY profilesChanged)
    Q_PROPERTY(QVariantList policyRules READ policyRules NOTIFY policyRulesChanged)
    Q_PROPERTY(int          deviceCount READ deviceCount NOTIFY profilesChanged)
    Q_PROPERTY(int          osCount     READ osCount     NOTIFY profilesChanged)

public:
    explicit SimulatorConfig(QObject* parent = nullptr) : QObject(parent) {}

    // ── Load all profiles from disk ─────────────────────────────────────
    // Hardcoded fallback ensures UI is never blank if no JSON files found.
    void loadAll(const QString& profilesDir) {
        loadDevices(profilesDir + QStringLiteral("/devices"));
        loadOSProfiles(profilesDir + QStringLiteral("/os"));
        loadPolicies(profilesDir + QStringLiteral("/test_policy"));
        // Fill any category that came up empty with defaults
        loadHardcodedDefaults();
        emit profilesChanged();
    }

    // ── Hardcoded fallback when no profile JSON files are found ───────
    void loadHardcodedDefaults() {
        if (m_devices.isEmpty()) {
            DeviceProfile d;
            d.id="win-x64"; d.name="Windows 11 (x64)"; d.os="windows";
            d.screenWidth=1024; d.screenHeight=640; d.bezel=0; d.radius=8;
            m_devices.append(d);
            QVariantMap dm; dm["id"]=d.id;dm["name"]=d.name;dm["os"]=d.os;
            dm["w"]=d.screenWidth;dm["h"]=d.screenHeight;dm["bezel"]=d.bezel;
            dm["island"]=d.island;dm["radius"]=d.radius;
            dm["pixelRatio"]=d.pixelRatio;dm["orientation"]=d.orientation;
            m_deviceList.append(dm);

            d.id="ios-iphone15pm"; d.name="iPhone 15 Pro Max"; d.os="ios";
            d.screenWidth=430; d.screenHeight=932; d.bezel=12; d.radius=55; d.island=true;
            m_devices.append(d);
            QVariantMap dm2; dm2["id"]=d.id;dm2["name"]=d.name;dm2["os"]=d.os;
            dm2["w"]=d.screenWidth;dm2["h"]=d.screenHeight;dm2["bezel"]=d.bezel;
            dm2["island"]=d.island;dm2["radius"]=d.radius;
            dm2["pixelRatio"]=d.pixelRatio;dm2["orientation"]=d.orientation;
            m_deviceList.append(dm2);
        }
        if (m_osProfiles.isEmpty()) {
            OSProfile o;
            o.id="windows"; o.name="Windows 11"; o.platform="windows";
            o.qtConfigRef="resources/config/windows.conf";
            o.defaultDevice="win-x64"; o.icon="windows"; o.color="#00A4EF";
            m_osProfiles.append(o);
            QVariantMap om; om["id"]=o.id;om["name"]=o.name;om["platform"]=o.platform;
            om["icon"]=o.icon;om["color"]=o.color;
            m_osList.append(om);
        }
    }

    // ── Device access ───────────────────────────────────────────────────
    QVariantList devices() const { return m_deviceList; }
    int deviceCount() const { return m_devices.size(); }

    const DeviceProfile* device(int idx) const {
        return (idx >= 0 && idx < m_devices.size()) ? &m_devices[idx] : nullptr;
    }
    const DeviceProfile* deviceById(const QString& id) const {
        for (const auto& d : m_devices)
            if (d.id == id) return &d;
        return nullptr;
    }
    int deviceIndex(const QString& id) const {
        for (int i = 0; i < m_devices.size(); ++i)
            if (m_devices[i].id == id) return i;
        return -1;
    }

    // ── OS access ───────────────────────────────────────────────────────
    QVariantList osList() const { return m_osList; }
    int osCount() const { return m_osProfiles.size(); }

    const OSProfile* osProfile(int idx) const {
        return (idx >= 0 && idx < m_osProfiles.size()) ? &m_osProfiles[idx] : nullptr;
    }
    const OSProfile* osProfileById(const QString& id) const {
        for (const auto& o : m_osProfiles)
            if (o.id == id) return &o;
        return nullptr;
    }

    // ── Policy access ───────────────────────────────────────────────────
    const TestPolicy* policyForPlatform(const QString& platform) const {
        for (const auto& p : m_policies)
            if (p.simulatedPlatform == platform) return &p;
        return nullptr;
    }

    // ── Policy rules for the active platform ──────────────────────────
    QVariantList policyRules() const { return m_policyRules; }

    Q_INVOKABLE void setActivePlatform(const QString& platform) {
        m_activePlatform = platform;
        m_policyRules.clear();
        const TestPolicy* p = policyForPlatform(platform);
        if (p) {
            for (const auto& r : p->skipRules) {
                QVariantMap m;
                m["diagId"]   = r.diagId;
                m["testName"] = r.testName;
                m["reason"]   = r.reason;
                m_policyRules.append(m);
            }
        }
        emit policyRulesChanged();
    }

    // ── Default device for an OS ────────────────────────────────────────
    int defaultDeviceIndexForOS(const QString& osId) const {
        const OSProfile* os = osProfileById(osId);
        if (os && !os->defaultDevice.isEmpty()) {
            int idx = deviceIndex(os->defaultDevice);
            if (idx >= 0) return idx;
        }
        // Fall back to first device matching this OS
        for (int i = 0; i < m_devices.size(); ++i)
            if (m_devices[i].os == osId) return i;
        return 0;
    }

signals:
    void profilesChanged();
    void policyRulesChanged();

private:
    bool loadDevices(const QString& dir) {
        m_devices.clear();
        m_deviceList.clear();
        QDir d(dir);
        if (!d.exists()) return false;
        for (const auto& fi : d.entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
            DeviceProfile dp = DeviceProfile::load(fi.absoluteFilePath());
            if (dp.isValid()) {
                m_devices.append(dp);
                QVariantMap m;
                m["id"]           = dp.id;
                m["name"]         = dp.name;
                m["os"]           = dp.os;
                m["w"]            = dp.screenWidth;
                m["h"]            = dp.screenHeight;
                m["bezel"]        = dp.bezel;
                m["island"]       = dp.island;
                m["radius"]       = dp.radius;
                m["pixelRatio"]   = dp.pixelRatio;
                m["orientation"]  = dp.orientation;
                m_deviceList.append(m);
            }
        }
        return !m_devices.isEmpty();
    }

    bool loadOSProfiles(const QString& dir) {
        m_osProfiles.clear();
        m_osList.clear();
        QDir d(dir);
        if (!d.exists()) return false;
        for (const auto& fi : d.entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
            OSProfile op = OSProfile::load(fi.absoluteFilePath());
            if (op.isValid()) {
                m_osProfiles.append(op);
                QVariantMap m;
                m["id"]            = op.id;
                m["name"]          = op.name;
                m["platform"]      = op.platform;
                m["qtConfigRef"]   = op.qtConfigRef;
                m["defaultDevice"] = op.defaultDevice;
                m["policyRef"]     = op.policyRef;
                m["icon"]          = op.icon;
                m["color"]         = op.color;
                m_osList.append(m);
            }
        }
        return !m_osProfiles.isEmpty();
    }

    bool loadPolicies(const QString& dir) {
        m_policies.clear();
        QDir d(dir);
        if (!d.exists()) return false;
        for (const auto& fi : d.entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
            TestPolicy tp = TestPolicy::load(fi.absoluteFilePath());
            if (tp.isValid())
                m_policies.append(tp);
        }
        return !m_policies.isEmpty();
    }

    QList<DeviceProfile> m_devices;
    QVariantList          m_deviceList;
    QList<OSProfile>      m_osProfiles;
    QVariantList          m_osList;
    QList<TestPolicy>     m_policies;
    QString               m_activePlatform;
    QVariantList          m_policyRules;
};

#include "moc_SimulatorConfig.cpp"
