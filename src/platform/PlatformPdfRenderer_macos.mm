// =============================================================================
// PlatformPdfRenderer_macos.mm — macOS PDF rendering via PDFKit (Quartz)
// =============================================================================
// PDFKit (PDFDocument/PDFPage) is part of macOS Quartz framework. Available
// on all macOS versions without extra dependencies. Rendering quality is
// identical to Preview.app since both use the same CoreGraphics backend.
//
// 5WHY: Desktop builds used QtPdf (rarely available) or image fallback
// (single static image, no pagination). macOS PDFKit gives desktop the
// same native PDF experience as iOS (CGPDFDocument) and Android (PdfRenderer).
#include "platform/PlatformPdfRenderer.h"

#import <PDFKit/PDFKit.h>
#import <CoreGraphics/CoreGraphics.h>

struct PlatformPdfRenderer::Impl {
    PDFDocument* doc = nil;
    int pages = 0;
};

PlatformPdfRenderer::PlatformPdfRenderer() : d(new Impl) {}
PlatformPdfRenderer::~PlatformPdfRenderer() { close(); delete d; }

bool PlatformPdfRenderer::load(const QString& filePath) {
    close();
    @autoreleasepool {
        NSURL* url = [NSURL fileURLWithPath:filePath.toNSString()];
        d->doc = [[PDFDocument alloc] initWithURL:url];
        if (!d->doc) return false;
        d->pages = (int)d->doc.pageCount;
        m_loaded = (d->pages > 0);
        return m_loaded;
    }
}

int PlatformPdfRenderer::pageCount() const {
    return d->pages;
}

QImage PlatformPdfRenderer::renderPage(int pageIndex, int width) const {
    if (!d->doc || pageIndex < 0 || pageIndex >= d->pages) return {};
    @autoreleasepool {
        PDFPage* page = [d->doc pageAtIndex:pageIndex];
        if (!page) return {};

        CGRect pageRect = [page boundsForBox:kPDFDisplayBoxMediaBox];
        if (pageRect.size.width <= 0.0f) return {}; // zero-width guard
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

        // PDF page coordinate system: origin bottom-left, y-up.
        // CGContext: origin top-left, y-down. Apply flip transform.
        CGContextTranslateCTM(ctx, 0, height);
        CGContextScaleCTM(ctx, scale, -scale);
        [page drawWithBox:kPDFDisplayBoxMediaBox toContext:ctx];
        CGContextRelease(ctx);
        return img;
    }
}

void PlatformPdfRenderer::close() {
    @autoreleasepool {
        if (d->doc) { [d->doc release]; d->doc = nil; }
        d->pages = 0;
        m_loaded = false;
    }
}
