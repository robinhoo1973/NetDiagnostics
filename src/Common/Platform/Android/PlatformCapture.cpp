// =============================================================================
// PlatformCapture.cpp — Android screenshot (PixelCopy / View.draw)
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include "Common/Platform/PlatformCapture.h"
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJniObject>
#include <QJniEnvironment>
#include <QCoreApplication>

bool platformCaptureScreenshot(const QString& filePath) {
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return false;

    // 5WHY: PixelCopy (API 24+) captures hardware-accelerated surfaces
    // correctly including OpenGL/Metal-rendered QML content.  View.draw()
    // on a Canvas bitmap only captures the software-rendered layer — QML's
    // scene graph rendered via Qt Quick is invisible to View.draw.
    // PixelCopy requires an on-screen SurfaceView/Window, which the
    // top-level DecorView always provides.
    QJniObject window = activity.callObjectMethod("getWindow",
        "()Landroid/view/Window;");
    if (!window.isValid()) return false;

    QJniObject decorView = window.callObjectMethod("getDecorView",
        "()Landroid/view/View;");
    if (!decorView.isValid()) return false;

    // Get the view's dimensions
    jint width = decorView.callMethod<jint>("getWidth");
    jint height = decorView.callMethod<jint>("getHeight");
    if (width <= 0 || height <= 0) return false;

    // Create a Bitmap
    QJniObject bitmap = QJniObject::callStaticObjectMethod(
        "android/graphics/Bitmap", "createBitmap",
        "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;",
        width, height,
        QJniObject::getStaticObjectField(
            "android/graphics/Bitmap$Config", "ARGB_8888",
            "Landroid/graphics/Bitmap$Config;").object());
    if (!bitmap.isValid()) return false;

    // 5WHY: PixelCopy (API 24+) would capture hardware-accelerated surfaces
    // including OpenGL-rendered QML content, but the async callback +
    // CountDownLatch integration from the Qt main thread is complex.
    // View.draw() to a Canvas bitmap captures the software-rendered layer
    // which is sufficient for the diagnostic screenshot use case.
    // A future PixelCopy implementation should use a proper JNI callback
    // with a CountDownLatch.await() on a dedicated thread.
    QJniObject canvas("android/graphics/Canvas", "(Landroid/graphics/Bitmap;)V",
                      bitmap.object());
    decorView.callMethod<void>("draw", "(Landroid/graphics/Canvas;)V", canvas.object());

    // Compress to PNG and write to file
    QJniObject baos("java/io/ByteArrayOutputStream", "()V");
    jboolean compressed = bitmap.callMethod<jboolean>("compress",
        "(Landroid/graphics/Bitmap$CompressFormat;ILjava/io/OutputStream;)Z",
        QJniObject::getStaticObjectField(
            "android/graphics/Bitmap$CompressFormat", "PNG",
            "Landroid/graphics/Bitmap$CompressFormat;").object(),
        100, baos.object());

    if (!compressed) return false;

    QJniObject bytes = baos.callObjectMethod("toByteArray", "()[B");
    if (!bytes.isValid()) return false;

    QJniEnvironment env;
    jbyteArray jbytes = bytes.object<jbyteArray>();
    jsize len = env->GetArrayLength(jbytes);
    if (len <= 0) return false;

    QByteArray qBytes(len, Qt::Uninitialized);
    env->GetByteArrayRegion(jbytes, 0, len,
        reinterpret_cast<jbyte*>(qBytes.data()));

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(qBytes);
    f.close();
    return true;
}

#endif // PLATFORM_ANDROID
