// =============================================================================
// CaptureFeatureGate.cpp — implementation
// =============================================================================
#include "Common/Model/CaptureFeatureGate.h"
#include <QSettings>

bool CaptureFeatureGate::isFeatureEnabled() {
    return QSettings().value(QString::fromLatin1(kSettingsKey), false).toBool();
}

void CaptureFeatureGate::setFeatureEnabled(bool enabled) {
    // 5WHY: Removed explicit QSettings::sync() — it triggers a synchronous
    // filesystem flush (NSUserDefaults synchronize on iOS, which Apple has
    // deprecated since iOS 12).  QSettings automatically persists within a
    // few seconds, and the feature gate changes only on double-click (rare
    // user interaction), making sync() unnecessary I/O on the main thread.
    QSettings s;
    s.setValue(QString::fromLatin1(kSettingsKey), enabled);
}

QString CaptureFeatureGate::disabledReason() {
    return QStringLiteral("Capture feature is disabled. "
                          "Double-click the app icon in Settings > About to enable.");
}
