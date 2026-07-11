// =============================================================================
// OSProfile.h — Operating-system profile for the simulator.
//
// Describes a simulated OS: its platform type, version, the production Qt
// config to reference for rendering, and a pointer to the policy file that
// controls test-skip rules.
// =============================================================================
#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>

struct OSProfile {
    QString  id;              // unique key: "ios", "android", "windows", "linux", "macos"
    QString  name;            // display: "iOS 18", "Android 15", "Windows 11"
    QString  platform;        // canonical platform tag matching DiagPlatform
    QString  qtConfigRef;     // path or key to production Qt config (qtquickcontrols2.conf)
    QString  defaultDevice;   // device id to pre-select
    QString  policyRef;       // path to test-policy JSON for this OS
    QString  icon;            // AppIcon name for UI
    QString  color;           // accent hex for UI

    bool isValid() const { return !id.isEmpty(); }

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"]            = id;
        o["name"]          = name;
        o["platform"]      = platform;
        o["qtConfigRef"]   = qtConfigRef;
        o["defaultDevice"] = defaultDevice;
        o["policyRef"]     = policyRef;
        o["icon"]          = icon;
        o["color"]         = color;
        return o;
    }

    static OSProfile fromJson(const QJsonObject& o) {
        OSProfile p;
        p.id            = o.value("id").toString();
        p.name          = o.value("name").toString();
        p.platform      = o.value("platform").toString();
        p.qtConfigRef   = o.value("qtConfigRef").toString();
        p.defaultDevice = o.value("defaultDevice").toString();
        p.policyRef     = o.value("policyRef").toString();
        p.icon          = o.value("icon").toString();
        p.color         = o.value("color").toString();
        return p;
    }

    static OSProfile load(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return fromJson(QJsonDocument::fromJson(f.readAll()).object());
    }
};
