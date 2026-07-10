// =============================================================================
// PlatformPdfRenderer.h — Platform-native PDF page rendering (mobile + desktop fallback)
// =============================================================================
// iOS:     CGPDFDocument (CoreGraphics, no extra framework needed)
// Android: PdfRenderer via JNI (API 21+)
// Desktop: QtPdf QPdfDocument is preferred; this is the fallback when QtPdf absent
//
// Renders individual PDF pages to QImage for display in a page-based QML viewer.
#pragma once
#include <QImage>
#include <QString>

class PlatformPdfRenderer {
public:
    PlatformPdfRenderer();
    ~PlatformPdfRenderer();

    // Load a PDF from file path. Returns false on failure.
    bool load(const QString& filePath);

    // Number of pages in the loaded document (0 if not loaded).
    int pageCount() const;

    // Render a single page (0-indexed) to QImage at the given width.
    // Height is derived from the page aspect ratio. Returns null QImage on failure.
    QImage renderPage(int pageIndex, int width = 800) const;

    // Release the loaded document.
    void close();

    bool isLoaded() const { return m_loaded; }

private:
    struct Impl;
    Impl* d = nullptr;
    bool m_loaded = false;
};
