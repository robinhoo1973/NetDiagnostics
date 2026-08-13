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

    // 5WHY: cppcheck flagged missing copy constructor/operator= for PIMPL class
    // managing Impl* via raw pointer. Copying would double-free the Impl.
    // Delete copy + move operations to enforce single-owner semantics.
    PlatformPdfRenderer(const PlatformPdfRenderer&) = delete;
    PlatformPdfRenderer& operator=(const PlatformPdfRenderer&) = delete;
    PlatformPdfRenderer(PlatformPdfRenderer&&) = delete;
    PlatformPdfRenderer& operator=(PlatformPdfRenderer&&) = delete;

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

// ── Desktop stub (Windows/Linux) — inline for zero-overhead fallback ────
#if !defined(__APPLE__) && !defined(PLATFORM_ANDROID)
struct PlatformPdfRenderer::Impl { int pages = 0; };
inline PlatformPdfRenderer::PlatformPdfRenderer() : d(new Impl) {}
inline PlatformPdfRenderer::~PlatformPdfRenderer() { close(); delete d; }
inline bool PlatformPdfRenderer::load(const QString&) { return false; }
inline int PlatformPdfRenderer::pageCount() const { return 0; }
inline QImage PlatformPdfRenderer::renderPage(int, int) const { return {}; }
inline void PlatformPdfRenderer::close() { m_loaded = false; }
#endif
