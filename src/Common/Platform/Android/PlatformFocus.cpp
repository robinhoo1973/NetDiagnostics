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

// 5WHY: QJniObject::callStaticObjectMethod("QtNative", "activity") was
// called at 7 sites — 3× in rapid succession during capture startup.
// Each call crosses the JNI boundary with thread-state transitions and
// reference-table bookkeeping (~1-3ms each).  However, the Activity
// MUST NOT be cached across calls because Android destroys and recreates
// the Activity on configuration changes (rotation, multi-window resize,
// locale change).  The JNI overhead (3 calls per capture startup) is
// negligible compared to the risk of operating on a destroyed Activity.
//
// 5WHY: Other Android platform files use QNativeInterface::QAndroidApplication::context(),
// which returns the application Context (not necessarily an Activity).
// Window-level operations (getWindow, setRequestedOrientation) require an
// Activity — the application Context does not have a Window.  We stay with
// QtNative::activity() which returns the QtActivity and is guaranteed to
// be an Activity subclass.
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

bool platformDisableFocusMode() {
    if (!s_focusEnabled) return false;

    QJniObject activity = getQtActivity();
    if (!activity.isValid()) {
        qWarning() << "PlatformFocus: Cannot get Qt activity to restore DND — will retry";
        // 5WHY: Keep s_focusEnabled=true on JNI failure — same pattern as
        // platformRestoreBrightness which preserves s_brightnessSaved.
        // The destructor safety net (restoreSystemState) provides a second
        // chance when the Activity recovers.  Only clear after success.
        return false;
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
    return true;
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

bool platformSetMaxBrightness() {
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) {
        qWarning() << "PlatformFocus: cannot get Activity to set max brightness";
        return false;
    }

    QJniObject window = activity.callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) {
        qWarning() << "PlatformFocus: cannot get Window to set max brightness";
        return false;
    }

    QJniObject attrs = window.callObjectMethod(
        "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
    if (!attrs.isValid()) {
        qWarning() << "PlatformFocus: cannot get LayoutParams to set max brightness";
        return false;
    }

    // Save current brightness (only on first call so we can restore the original)
    if (!s_brightnessSaved) {
        s_savedBrightness = attrs.getField<jfloat>("screenBrightness");
        s_brightnessSaved = true;
    }

    // Set max brightness (1.0f = 100%)
    attrs.setField("screenBrightness", 1.0f);
    window.callMethod<void>("setAttributes",
        "(Landroid/view/WindowManager$LayoutParams;)V", attrs.object());
    return true;
}

bool platformRestoreBrightness() {
    if (!s_brightnessSaved) return false;

    QJniObject activity = getQtActivity();
    // 5WHY: Previously cleared s_brightnessSaved on every JNI failure path.
    // If the Activity was temporarily invalid (app backgrounded during capture
    // teardown), the saved value was abandoned.  On the next capture,
    // platformSetMaxBrightness() saw s_brightnessSaved==false and re-saved the
    // *current* brightness — which was still 1.0 from the failed restore —
    // permanently losing the user's original brightness.
    //
    // Now we KEEP s_brightnessSaved on JNI failure.  restoreSystemState() is
    // called from the destructor as a safety net, giving a second chance when
    // the Activity recovers.  Only clear s_brightnessSaved after a successful
    // restore.
    if (!activity.isValid()) {
        qWarning() << "PlatformFocus: cannot get Activity for brightness restore — will retry";
        return false;
    }

    QJniObject window = activity.callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) {
        qWarning() << "PlatformFocus: cannot get Window for brightness restore — will retry";
        return false;
    }

    QJniObject attrs = window.callObjectMethod(
        "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
    if (!attrs.isValid()) {
        qWarning() << "PlatformFocus: cannot get LayoutParams for brightness restore — will retry";
        return false;
    }

    attrs.setField("screenBrightness", s_savedBrightness);
    window.callMethod<void>("setAttributes",
        "(Landroid/view/WindowManager$LayoutParams;)V", attrs.object());
    s_brightnessSaved = false;
    return true;
}

// ── Orientation lock ────────────────────────────────────────────────
// 5WHY: SCREEN_ORIENTATION_LOCKED = 14 (API 18+). Restore to
// SCREEN_ORIENTATION_USER = 2 to resume system-managed rotation.
bool platformLockOrientation() {
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) {
        qWarning() << "PlatformFocus: cannot get Activity to lock orientation";
        return false;
    }
    QJniEnvironment env;
    activity.callMethod<void>("setRequestedOrientation", "(I)V", 14);
    // 5WHY: setRequestedOrientation returns void — a successful JNI call
    // does not guarantee the framework honoured the request (Activity may
    // be finishing during a configuration change).  Check for JNI exceptions
    // at least; a silent JNI throw means the call was discarded entirely.
    if (env.checkAndClearExceptions()) {
        qWarning() << "PlatformFocus: setRequestedOrientation(LOCKED) threw JNI exception";
        return false;
    }
    return true;
}

bool platformUnlockOrientation() {
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) {
        qWarning() << "PlatformFocus: cannot get Activity to unlock orientation";
        return false;
    }
    QJniEnvironment env;
    activity.callMethod<void>("setRequestedOrientation", "(I)V", 2);
    if (env.checkAndClearExceptions()) {
        qWarning() << "PlatformFocus: setRequestedOrientation(USER) threw JNI exception";
        return false;
    }
    return true;
}

void platformOpenFocusSettings() {
    // 5WHY: startActivity from a non-Activity context (our C++ is
    // called from QML, not from an Activity subclass) requires the
    // FLAG_ACTIVITY_NEW_TASK flag, or Android throws an exception.
    //
    // 5WHY: NOTIFICATION_POLICY_ACCESS_SETTINGS (API 23+) opens the
    // "Do Not Disturb access" page where the user can grant this app
    // permission to control DND.  NOTIFICATION_SETTINGS opens the
    // app's notification channel settings — the wrong page for this
    // purpose, since the user needs to grant DND access, not manage
    // notification channels.
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) {
        qWarning() << "PlatformFocus: cannot get Activity to open focus settings";
        return;
    }
    QJniObject intent("android/content/Intent",
        "(Ljava/lang/String;)V",
        QJniObject::fromString("android.settings.NOTIFICATION_POLICY_ACCESS_SETTINGS").object());
    if (!intent.isValid()) {
        // 5WHY: On custom Android ROMs (Samsung, Huawei, Xiaomi) the
        // NOTIFICATION_POLICY_ACCESS_SETTINGS action may not be registered.
        // Log a warning so the user/developer knows why the Settings page
        // didn't open when they tapped the DND hint in the preflight overlay.
        qWarning() << "PlatformFocus: cannot construct DND settings Intent — "
                       "NOTIFICATION_POLICY_ACCESS_SETTINGS may not be "
                       "supported on this device";
        return;
    }
    intent.callMethod<void>("addFlags", "(I)V", 0x10000000); // FLAG_ACTIVITY_NEW_TASK
    activity.callMethod<void>("startActivity",
        "(Landroid/content/Intent;)V", intent.object());
}
