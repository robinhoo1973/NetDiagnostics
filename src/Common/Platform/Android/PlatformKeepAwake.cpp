// =============================================================================
// PlatformKeepAwake.cpp — Android screen keep-awake (FLAG_KEEP_SCREEN_ON)
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include "Common/Platform/PlatformKeepAwake.h"
#include <QJniObject>
#include <QCoreApplication>

static bool s_keepAwake = false;

// 5WHY: android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON = 0x00000080
// Using the constant value directly avoids a JNI static-field lookup on every
// toggle.  If this flag value ever changes in a future Android SDK (unlikely
// since it's been stable since API 1), this would need to be updated.
static constexpr jint kFlagKeepScreenOn = 0x00000080;

void platformSetKeepAwake(bool enable) {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return;

    QJniObject window = activity.callObjectMethod("getWindow",
        "()Landroid/view/Window;");
    if (!window.isValid()) return;

    if (enable) {
        window.callMethod<void>("addFlags", "(I)V", kFlagKeepScreenOn);
    } else {
        window.callMethod<void>("clearFlags", "(I)V", kFlagKeepScreenOn);
    }
    s_keepAwake = enable;
}

bool platformIsKeepAwake() {
    return s_keepAwake;
}

#endif // PLATFORM_ANDROID
