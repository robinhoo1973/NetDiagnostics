// =============================================================================
// PlatformPdfRenderer_ios.mm — iOS PDF rendering via CoreGraphics
// =============================================================================
// Uses CGPDFDocument (CoreGraphics) — available on all iOS versions, no
// extra framework link needed (CoreGraphics is part of every iOS SDK).
#include "Common/Platform/PlatformPdfRenderer.h"
#include "Common/Platform/Apple/PlatformPdfRenderer_cg.h"
#import <CoreGraphics/CoreGraphics.h>

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
    // Shared CoreGraphics boilerplate extracted to PlatformPdfRenderer_cg.h
    return renderPageViaCoreGraphics(page, width,
        [page](CGContextRef ctx) { CGContextDrawPDFPage(ctx, page); });
}

void PlatformPdfRenderer::close() {
    if (d->doc) { CGPDFDocumentRelease(d->doc); d->doc = nullptr; }
    d->pages = 0;
    m_loaded = false;
}
