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
