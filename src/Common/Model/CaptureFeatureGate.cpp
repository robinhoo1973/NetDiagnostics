// =============================================================================
// CaptureFeatureGate.cpp — implementation
// =============================================================================
#include "Common/Model/CaptureFeatureGate.h"
#include <QSettings>

bool CaptureFeatureGate::isFeatureEnabled() {
    return QSettings().value(QString::fromLatin1(kSettingsKey), false).toBool();
}

void CaptureFeatureGate::setFeatureEnabled(bool enabled) {
    QSettings s;
    s.setValue(QString::fromLatin1(kSettingsKey), enabled);
    s.sync();
}

QString CaptureFeatureGate::disabledReason() {
    return QStringLiteral("Capture feature is disabled. "
                          "Double-click the app icon in Settings > About to enable.");
}
