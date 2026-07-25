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

bool platformEnableFocusMode() {
    if (s_focusEnabled) return true;

    // Get the NotificationManager service
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "getActivity",
        "()Landroid/app/Activity;");
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

    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "getActivity",
        "()Landroid/app/Activity;");
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

void platformSetMaxBrightness() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!activity.isValid()) return;

    QJniObject window = activity.callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) return;

    QJniObject attrs = window.callObjectMethod(
        "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
    if (!attrs.isValid()) return;

    // Save current brightness
    jfloat current = attrs.getField<jfloat>("screenBrightness");
    if (s_savedBrightness < 0) s_savedBrightness = current;

    // Set max brightness (1.0f = 100%)
    attrs.setField("screenBrightness", 1.0f);
    window.callMethod<void>("setAttributes",
        "(Landroid/view/WindowManager$LayoutParams;)V", attrs.object());
}

void platformRestoreBrightness() {
    if (s_savedBrightness < 0) return;

    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!activity.isValid()) return;

    QJniObject window = activity.callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) return;

    QJniObject attrs = window.callObjectMethod(
        "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
    if (!attrs.isValid()) return;

    attrs.setField("screenBrightness", s_savedBrightness);
    window.callMethod<void>("setAttributes",
        "(Landroid/view/WindowManager$LayoutParams;)V", attrs.object());
    s_savedBrightness = -1.0f;
}
