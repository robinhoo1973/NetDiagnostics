// =============================================================================
// CaptureFeatureGate.h — Hidden capture feature toggle (screenshot/recording)
// =============================================================================
// Activated by double-clicking the app icon in Settings > About.
// State persisted via QSettings; default is disabled (false) in all builds.
//
// Design ref: review/06_Capture_Architecture_Design.md §2.1
// =============================================================================
#pragma once

#include <QString>

class CaptureFeatureGate {
public:
    // Release default: false. No auto-enable in debug builds — the hidden
    // double-click activation is the sole entry point.
    static bool isFeatureEnabled();

    // Toggle the feature on/off. Persisted to QSettings immediately.
    static void setFeatureEnabled(bool enabled);

    // Human-readable reason when the feature is disabled.
    static QString disabledReason();

private:
    static constexpr const char* kSettingsKey = "capture/featureEnabled";
};
