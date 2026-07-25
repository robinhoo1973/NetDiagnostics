// =============================================================================
// PlatformFocus.cpp — Android focus / Do-Not-Disturb implementation
// =============================================================================
// Uses NotificationManager.setInterruptionFilter() (API 23+) to enable
// PRIORITY-only DND mode, which suppresses all non-priority notifications.
// Requires the `android.permission.ACCESS_NOTIFICATION_POLICY` permission.
//
// Reference:
//   https://developer.android.com/reference/android/app/NotificationManager
// =============================================================================
#include "Common/Platform/PlatformFocus.h"
#include <QJniObject>
#include <QJniEnvironment>
#include <QtDebug>

static bool s_focusEnabled = false;
static int s_originalFilter = -1;  // saved interruption filter

// 5WHY: QtNative activity acquisition duplicated in enableFocusMode,
// disableFocusMode, setMaxBrightness, and restoreBrightness (4x the same
// 5-line JNI pattern).  Extract once.
static QJniObject getQtActivity() {
    return QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
}

bool platformEnableFocusMode() {
    if (s_focusEnabled) return true;

    // Get the NotificationManager service
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) {
        qWarning() << "PlatformFocus: Cannot get Qt activity";
        return false;
    }

    QJniObject notificationManager = activity.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::getStaticObjectField(
            "android/content/Context",
            "NOTIFICATION_SERVICE",
            "Ljava/lang/String;").object());
    if (!notificationManager.isValid()) {
        qWarning() << "PlatformFocus: Cannot get NotificationManager";
        return false;
    }

    // Check if notification policy access has been granted by the user.
    // setInterruptionFilter() requires ACCESS_NOTIFICATION_POLICY which is
    // a special app-ops permission — the user must grant it in Settings.
    jboolean hasPolicy = notificationManager.callMethod<jboolean>(
        "isNotificationPolicyAccessGranted");
    if (!hasPolicy) {
        qWarning() << "PlatformFocus: ACCESS_NOTIFICATION_POLICY not granted — DND unavailable";
        return false;
    }

    // Save current interruption filter
    s_originalFilter = notificationManager.callMethod<jint>(
        "getCurrentInterruptionFilter");

    // Set to PRIORITY-only DND (suppresses non-priority notifications)
    jint priorityFilter = notificationManager.getStaticField<jint>(
        "android/app/NotificationManager",
        "INTERRUPTION_FILTER_PRIORITY");
    notificationManager.callMethod<void>(
        "setInterruptionFilter", "(I)V", priorityFilter);

    s_focusEnabled = true;
    qInfo() << "PlatformFocus: DND enabled (priority filter)";
    return true;
}

void platformDisableFocusMode() {
    if (!s_focusEnabled) return;

    QJniObject activity = getQtActivity();
    if (!activity.isValid()) {
        qWarning() << "PlatformFocus: Cannot get Qt activity to restore DND";
        s_focusEnabled = false;
        return;
    }

    QJniObject notificationManager = activity.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::getStaticObjectField(
            "android/content/Context",
            "NOTIFICATION_SERVICE",
            "Ljava/lang/String;").object());
    if (notificationManager.isValid() && s_originalFilter >= 0) {
        // Restore original interruption filter
        notificationManager.callMethod<void>(
            "setInterruptionFilter", "(I)V", s_originalFilter);
        s_originalFilter = -1;
    }

    s_focusEnabled = false;
    qInfo() << "PlatformFocus: DND disabled";
}

bool platformIsFocusModeEnabled() {
    return s_focusEnabled;
}

// ── Brightness: uses WindowManager.LayoutParams (best-effort) ────────────
// 5WHY: Settings.System requires WRITE_SETTINGS permission which is not
// guaranteed. Use the window-level brightness instead — works from any
// Activity context without additional permissions.
static float s_savedBrightness = -1.0f;
// 5WHY: -1.0f is also Android's BRIGHTNESS_OVERRIDE_NONE (use system
// brightness), so we cannot use < 0 as a "not yet saved" sentinel.
// Track separately with a boolean.
static bool s_brightnessSaved = false;

void platformSetMaxBrightness() {
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) return;

    QJniObject window = activity.callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) return;

    QJniObject attrs = window.callObjectMethod(
        "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
    if (!attrs.isValid()) return;

    // Save current brightness (only on first call so we can restore the original)
    if (!s_brightnessSaved) {
        s_savedBrightness = attrs.getField<jfloat>("screenBrightness");
        s_brightnessSaved = true;
    }

    // Set max brightness (1.0f = 100%)
    attrs.setField("screenBrightness", 1.0f);
    window.callMethod<void>("setAttributes",
        "(Landroid/view/WindowManager$LayoutParams;)V", attrs.object());
}

void platformRestoreBrightness() {
    if (!s_brightnessSaved) return;

    QJniObject activity = getQtActivity();
    // 5WHY: If any JNI call fails (activity destroyed, app in background),
    // clear s_brightnessSaved so a future capture doesn't restore a stale brightness.
    if (!activity.isValid()) {
        s_brightnessSaved = false;
        return;
    }

    QJniObject window = activity.callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) {
        s_brightnessSaved = false;
        return;
    }

    QJniObject attrs = window.callObjectMethod(
        "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
    if (!attrs.isValid()) {
        s_brightnessSaved = false;
        return;
    }

    attrs.setField("screenBrightness", s_savedBrightness);
    window.callMethod<void>("setAttributes",
        "(Landroid/view/WindowManager$LayoutParams;)V", attrs.object());
    s_brightnessSaved = false;
}

// ── Orientation lock ────────────────────────────────────────────────
// 5WHY: SCREEN_ORIENTATION_LOCKED = 14 (API 18+). Restore to
// SCREEN_ORIENTATION_USER = 2 to resume system-managed rotation.
void platformLockOrientation() {
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) return;
    activity.callMethod<void>("setRequestedOrientation", "(I)V", 14);
}

void platformUnlockOrientation() {
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) return;
    activity.callMethod<void>("setRequestedOrientation", "(I)V", 2);
}

void platformOpenFocusSettings() {
    // 5WHY: startActivity from a non-Activity context (our C++ is
    // called from QML, not from an Activity subclass) requires the
    // FLAG_ACTIVITY_NEW_TASK flag, or Android throws an exception.
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) return;
    QJniObject intent("android/content/Intent",
        "(Ljava/lang/String;)V",
        QJniObject::fromString("android.settings.NOTIFICATION_SETTINGS").object());
    intent.callMethod<void>("addFlags", "(I)V", 0x10000000); // FLAG_ACTIVITY_NEW_TASK
    activity.callMethod<void>("startActivity",
        "(Landroid/content/Intent;)V", intent.object());
}
