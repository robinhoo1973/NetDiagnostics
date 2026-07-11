// =============================================================================
// PlatformPdfRenderer_android.cpp — Android PDF rendering via PdfRenderer JNI
// =============================================================================
// Uses android.graphics.pdf.PdfRenderer (API 21+) to render pages to Bitmap,
// then copies pixel data to QImage. Requires a ParcelFileDescriptor for the
// PDF file — uses java.io.FileInputStream to obtain the FD.
#include "Common/Platform/PlatformPdfRenderer.h"

#if defined(PLATFORM_ANDROID)
#include <QJniObject>
#include <QJniEnvironment>
#include <jni.h>
#include <android/bitmap.h>

struct PlatformPdfRenderer::Impl {
    // Cached JNI class refs
    jclass pdfRendererClass = nullptr;
    jobject renderer = nullptr; // PdfRenderer instance
    jmethodID renderPageMethod = nullptr;
    jmethodID getPageCountMethod = nullptr;
    jmethodID closeMethod = nullptr;
    int pages = 0;
};

PlatformPdfRenderer::PlatformPdfRenderer() : d(new Impl) {}
PlatformPdfRenderer::~PlatformPdfRenderer() { close(); delete d; }

bool PlatformPdfRenderer::load(const QString& filePath) {
    close();
    QJniEnvironment env;
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    // android/os/ParcelFileDescriptor
    QJniObject jPath = QJniObject::fromString(filePath);
    QJniObject jFile("java/io/File", "(Ljava/lang/String;)V", jPath.object<jstring>());
    if (!jFile.isValid()) return false;

    QJniObject fd = QJniObject::callStaticObjectMethod(
        "android/os/ParcelFileDescriptor",
        "open", "(Ljava/io/File;I)Landroid/os/ParcelFileDescriptor;",
        jFile.object<jobject>(), 0x10000000 /* MODE_READ_ONLY */);
    if (!fd.isValid()) return false;

    // android/graphics/pdf/PdfRenderer
    QJniObject renderer("android/graphics/pdf/PdfRenderer",
        "(Landroid/os/ParcelFileDescriptor;)V", fd.object<jobject>());
    if (!renderer.isValid()) {
        // 5WHY: PdfRenderer construction failed but ParcelFileDescriptor
        // was already opened. Close it to prevent FD leak.
        fd.callMethod<void>("close");
        return false;
    }

    d->renderer = env->NewGlobalRef(renderer.object<jobject>());
    d->pages = renderer.callMethod<jint>("getPageCount");
    m_loaded = (d->pages > 0);
    return m_loaded;
}

int PlatformPdfRenderer::pageCount() const {
    return d->pages;
}

QImage PlatformPdfRenderer::renderPage(int pageIndex, int width) const {
    if (!d->renderer || pageIndex < 0 || pageIndex >= d->pages) return {};
    QJniEnvironment env;

    QJniObject renderer(d->renderer); // wrap the global ref
    // Open the page
    QJniObject pdfPage = renderer.callObjectMethod(
        "openPage", "(I)Landroid/graphics/pdf/PdfRenderer$Page;", pageIndex);
    if (!pdfPage.isValid()) return {};

    // Get page dimensions
    jint pw = pdfPage.callMethod<jint>("getWidth");
    jint ph = pdfPage.callMethod<jint>("getHeight");
    // 5WHY: Corrupt PDF with zero-width page -> division by zero.
    if (pw <= 0) { pdfPage.callMethod<void>("close"); return {}; }
    float scale = (float)width / (float)pw;
    int height = (int)(ph * scale);
    if (height < 1) height = 1;

    // Create Android Bitmap
    QJniObject config = QJniObject::getStaticObjectField(
        "android/graphics/Bitmap$Config", "ARGB_8888",
        "Landroid/graphics/Bitmap$Config;");
    QJniObject bitmap = QJniObject::callStaticObjectMethod(
        "android/graphics/Bitmap", "createBitmap",
        "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;",
        width, height, config.object<jobject>());
    if (!bitmap.isValid()) return {};

    // Render page into bitmap
    pdfPage.callMethod<void>("render",
        "(Landroid/graphics/Bitmap;Landroid/graphics/Rect;"
        "Landroid/graphics/Matrix;I)V",
        bitmap.object<jobject>(),
        nullptr, nullptr, 0 /* RENDER_MODE_FOR_DISPLAY */);

    // Close page
    pdfPage.callMethod<void>("close");

    // Lock pixels and copy to QImage
    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap.object<jobject>(), &info) < 0) return {};
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap.object<jobject>(), &pixels) < 0) return {};

    QImage img((const uchar*)pixels, width, height, QImage::Format_ARGB32_Premultiplied);
    QImage copy = img.copy(); // deep copy before unlocking

    AndroidBitmap_unlockPixels(env, bitmap.object<jobject>());
    return copy;
}

void PlatformPdfRenderer::close() {
    if (d->renderer) {
        QJniObject renderer(d->renderer);
        renderer.callMethod<void>("close");
        QJniEnvironment env;
        env->DeleteGlobalRef(d->renderer);
        d->renderer = nullptr;
    }
    d->pages = 0;
    m_loaded = false;
}
#endif // PLATFORM_ANDROID
