// =============================================================================
// PlatformPdfRenderer.cpp — Windows/Linux stub (QtPdf or image fallback)
// =============================================================================
// Desktop platforms without native PDF API use QtPdf (PdfScrollablePageView)
// when available, or fall back to the QTextDocument→QImage image preview.
// This stub provides the PlatformPdfRenderer interface for compilation only.
#include "platform/PlatformPdfRenderer.h"

struct PlatformPdfRenderer::Impl { int pages = 0; };
PlatformPdfRenderer::PlatformPdfRenderer() : d(new Impl) {}
PlatformPdfRenderer::~PlatformPdfRenderer() { close(); delete d; }
bool PlatformPdfRenderer::load(const QString&) { return false; }
int PlatformPdfRenderer::pageCount() const { return 0; }
QImage PlatformPdfRenderer::renderPage(int, int) const { return {}; }
void PlatformPdfRenderer::close() { m_loaded = false; }
