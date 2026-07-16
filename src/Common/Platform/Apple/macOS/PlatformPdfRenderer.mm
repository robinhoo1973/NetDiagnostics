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
#include "Common/Platform/PlatformPdfRenderer.h"
#include "Common/Platform/Apple/PlatformPdfRenderer_cg.h"

#import <PDFKit/PDFKit.h>

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
        // PDFKit 10.14+ exposes the backing CGPDFPageRef, enabling shared
        // CoreGraphics rendering with iOS — eliminates ~15 lines of dup code.
        CGPDFPageRef pageRef = [page pageRef];
        if (!pageRef) return {};
        return renderPageViaCoreGraphics(pageRef, width,
            [pageRef](CGContextRef ctx) { CGContextDrawPDFPage(ctx, pageRef); });
    }
}

void PlatformPdfRenderer::close() {
    @autoreleasepool {
        // 5WHY: manual [d->doc release] is incompatible with ARC (modern
        // Apple targets). Setting to nil releases under both ARC and non-ARC.
        d->doc = nil;
        d->pages = 0;
        m_loaded = false;
    }
}
