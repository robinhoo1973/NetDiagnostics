// =============================================================================
// PlatformKeepAwake.cpp — Android screen keep-awake (FLAG_KEEP_SCREEN_ON)
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include "Common/Platform/PlatformKeepAwake.h"
#include <QJniObject>
#include <QCoreApplication>

static bool s_keepAwake = false;

void platformSetKeepAwake(bool enable) {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return;

    QJniObject window = activity.callObjectMethod("getWindow",
        "()Landroid/view/Window;");
    if (!window.isValid()) return;

    if (enable) {
        window.callMethod<void>("addFlags", "(I)V",
            static_cast<jint>(0x00000080)); // FLAG_KEEP_SCREEN_ON = 0x80
    } else {
        window.callMethod<void>("clearFlags", "(I)V",
            static_cast<jint>(0x00000080));
    }
    s_keepAwake = enable;
}

bool platformIsKeepAwake() {
    return s_keepAwake;
}

#endif // PLATFORM_ANDROID
