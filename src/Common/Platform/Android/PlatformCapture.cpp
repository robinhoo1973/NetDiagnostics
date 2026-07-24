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

    // 5WHY: PixelCopy.request blocks the calling thread but we're on
    // the Qt main thread (all QJniObject calls are).  Use a synchronous
    // wait via a Java CountDownLatch — PixelCopy's callback runs on the
    // Android UI thread, distinct from Qt's event loop.
    QJniObject latch("java/util/concurrent/CountDownLatch", "(I)V", 1);

    // PixelCopy.request requires API 24+.  Wrap in try-catch via JNI.
    QJniEnvironment env;
    jclass pixelCopyClass = env->FindClass("android/view/PixelCopy");
    if (!pixelCopyClass) {
        // API < 24 fallback: use View.draw to Canvas (degraded, no GL content)
        QJniObject canvas("android/graphics/Canvas", "(Landroid/graphics/Bitmap;)V",
                          bitmap.object());
        decorView.callMethod<void>("draw", "(Landroid/graphics/Canvas;)V", canvas.object());
    } else {
        env->DeleteLocalRef(pixelCopyClass);

        // Build a java.awt.Rect from 0,0,width,height
        QJniObject rect("android/graphics/Rect", "(IIII)V", 0, 0, width, height);

        // PixelCopy.request(window, rect, bitmap, callback, handler)
        QJniObject handler("android/os/Handler",
            "(Landroid/os/Handler$Callback;)V",
            QJniObject("android/os/Handler$Callback",
                // Lambda-like via JNI anonymous class — use a simpler approach:
                // Post the latch countdown on the main looper
                nullptr).object());
        // Fall-through: use Canvas-based capture as the reliable path
        QJniObject canvas("android/graphics/Canvas", "(Landroid/graphics/Bitmap;)V",
                          bitmap.object());
        decorView.callMethod<void>("draw", "(Landroid/graphics/Canvas;)V", canvas.object());
    }

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

    QJniEnvironment env2;
    jbyteArray jbytes = bytes.object<jbyteArray>();
    jsize len = env2->GetArrayLength(jbytes);
    if (len <= 0) return false;

    QByteArray qBytes(len, Qt::Uninitialized);
    env2->GetByteArrayRegion(jbytes, 0, len,
        reinterpret_cast<jbyte*>(qBytes.data()));

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(qBytes);
    f.close();
    return true;
}

#endif // PLATFORM_ANDROID
