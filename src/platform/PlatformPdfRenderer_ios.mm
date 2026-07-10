// =============================================================================
// PlatformPdfRenderer_ios.mm — iOS PDF rendering via CoreGraphics
// =============================================================================
// Uses CGPDFDocument (CoreGraphics) — available on all iOS versions, no
// extra framework link needed (CoreGraphics is part of every iOS SDK).
#include "platform/PlatformPdfRenderer.h"
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

struct PlatformPdfRenderer::Impl {
    CGPDFDocumentRef doc = nullptr;
    int pages = 0;
};

PlatformPdfRenderer::PlatformPdfRenderer() : d(new Impl) {}
PlatformPdfRenderer::~PlatformPdfRenderer() { close(); delete d; }

bool PlatformPdfRenderer::load(const QString& filePath) {
    close();
    QByteArray utf8 = filePath.toUtf8();
    CFStringRef pathStr = CFStringCreateWithBytes(
        kCFAllocatorDefault, (const UInt8*)utf8.constData(),
        utf8.size(), kCFStringEncodingUTF8, false);
    CFURLRef url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault, pathStr, kCFURLPOSIXPathStyle, false);
    d->doc = CGPDFDocumentCreateWithURL(url);
    CFRelease(url);
    CFRelease(pathStr);
    if (!d->doc) return false;
    d->pages = (int)CGPDFDocumentGetNumberOfPages(d->doc);
    m_loaded = (d->pages > 0);
    return m_loaded;
}

int PlatformPdfRenderer::pageCount() const {
    return d->pages;
}

QImage PlatformPdfRenderer::renderPage(int pageIndex, int width) const {
    if (!d->doc || pageIndex < 0 || pageIndex >= d->pages) return {};
    CGPDFPageRef page = CGPDFDocumentGetPage(d->doc, pageIndex + 1); // 1-indexed
    if (!page) return {};
    CGRect pageRect = CGPDFPageGetBoxRect(page, kCGPDFMediaBox);
    // 5WHY: Corrupt PDF with zero-width page → division by zero → inf/nan.
    if (pageRect.size.width <= 0.0f) return {};
    float scale = (float)width / (float)pageRect.size.width;
    int height = (int)(pageRect.size.height * scale);
    if (height < 1) height = 1;

    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        img.bits(), width, height, 8, img.bytesPerLine(),
        cs, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(cs);
    if (!ctx) return {};

    // Flip Y axis (PDF origin is bottom-left; CGContext origin is top-left)
    CGContextTranslateCTM(ctx, 0, height);
    CGContextScaleCTM(ctx, scale, -scale);
    CGContextDrawPDFPage(ctx, page);
    CGContextRelease(ctx);
    return img;
}

void PlatformPdfRenderer::close() {
    if (d->doc) { CGPDFDocumentRelease(d->doc); d->doc = nullptr; }
    d->pages = 0;
    m_loaded = false;
}
