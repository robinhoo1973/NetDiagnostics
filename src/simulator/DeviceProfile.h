// =============================================================================
// DeviceProfile.h — Device profile data model for the simulator.
//
// Each device profile describes a target device's screen geometry, bezels,
// notch/island presence, pixel ratio, and frame style — everything needed
// to render an accurate device mockup in the DeviceViewport.
// =============================================================================
#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>

struct DeviceProfile {
    QString  id;            // unique key: "ios-iphone15pm", "android-pixel9", etc.
    QString  name;          // display name: "iPhone 15 Pro Max"
    QString  os;            // "ios" | "android" | "windows" | "linux" | "macos"
    int      screenWidth  = 430;
    int      screenHeight = 932;
    int      bezel        = 0;    // frame thickness in logical pixels
    int      radius       = 0;    // corner radius
    bool     island       = false;// dynamic island / notch
    qreal    pixelRatio   = 1.0;
    QString  orientation  = "portrait";  // "portrait" | "landscape"

    bool isValid() const { return !id.isEmpty() && screenWidth > 0 && screenHeight > 0; }

    // ── Serialisation ────────────────────────────────────────────────────
    QJsonObject toJson() const {
        QJsonObject o;
        o["id"]           = id;
        o["name"]         = name;
        o["os"]           = os;
        o["screenWidth"]  = screenWidth;
        o["screenHeight"] = screenHeight;
        o["bezel"]        = bezel;
        o["radius"]       = radius;
        o["island"]       = island;
        o["pixelRatio"]   = pixelRatio;
        o["orientation"]  = orientation;
        return o;
    }

    static DeviceProfile fromJson(const QJsonObject& o) {
        DeviceProfile d;
        d.id           = o.value("id").toString();
        d.name         = o.value("name").toString();
        d.os           = o.value("os").toString();
        d.screenWidth  = o.value("screenWidth").toInt(430);
        d.screenHeight = o.value("screenHeight").toInt(932);
        d.bezel        = o.value("bezel").toInt(0);
        d.radius       = o.value("radius").toInt(0);
        d.island       = o.value("island").toBool(false);
        d.pixelRatio   = o.value("pixelRatio").toDouble(1.0);
        d.orientation  = o.value("orientation").toString("portrait");
        return d;
    }

    static DeviceProfile load(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return fromJson(QJsonDocument::fromJson(f.readAll()).object());
    }
};
