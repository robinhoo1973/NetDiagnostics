// =============================================================================
// PlatformPdfRenderer_cg.h — Shared CoreGraphics rendering helper (Apple only)
// =============================================================================
// 5WHY: iOS (CGPDFDocument) and macOS (PDFKit) both render PDF pages to QImage
// via CoreGraphics. The QImage+CGContext setup and flip-transform boilerplate
// was duplicated (~15 lines each). Extract into a shared inline helper.
// Compiled only on Apple platforms (__APPLE__).
#pragma once

#if defined(__APPLE__)
#include <QImage>
#include <CoreGraphics/CoreGraphics.h>

// Renders a CGPDFPage to QImage using CoreGraphics.
// page: the PDF page to render (CGPDFPageRef or compatible CG drawable)
// drawFn: lambda that draws the page into the prepared CGContext
//         e.g. [](CGContextRef ctx) { CGContextDrawPDFPage(ctx, page); }
//         or   [&](CGContextRef ctx) { [pdfPage drawWithBox:... toContext:ctx]; }
template<typename DrawFn>
static inline QImage renderPageViaCoreGraphics(CGPDFPageRef page, int width,
                                                DrawFn drawFn) {
    CGRect pageRect = CGPDFPageGetBoxRect(page, kCGPDFMediaBox);
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

    // Flip Y axis (PDF origin bottom-left; CGContext origin top-left)
    CGContextTranslateCTM(ctx, 0, height);
    CGContextScaleCTM(ctx, scale, -scale);
    drawFn(ctx);
    CGContextRelease(ctx);
    return img;
}
#endif // __APPLE__
