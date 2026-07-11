// =============================================================================
// NativePdfDocument.cpp — QML-friendly PDF document wrapper
// =============================================================================
#include "Common/Platform/NativePdfDocument.h"
#include <QBuffer>
#include <QFileInfo>

NativePdfDocument::NativePdfDocument(QObject* parent) : QObject(parent) {}
NativePdfDocument::~NativePdfDocument() = default;

void NativePdfDocument::setSource(const QUrl& url) {
    // 5WHY: if (m_source == url) return skipped reload when file content
    // changed under the same path (e.g. theme toggle regenerates PDF).
    // Always close + reload to ensure fresh content.
    m_renderer.close();
    m_source = url;
    emit sourceChanged();
    m_loaded = false;
    m_pageCount = 0;

    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        emit errorOccurred(QStringLiteral("PDF file not found: %1").arg(path));
        return;
    }
    if (m_renderer.load(path)) {
        m_pageCount = m_renderer.pageCount();
        m_loaded = true;
        emit pageCountChanged();
        emit loadedChanged();
    } else {
        emit errorOccurred(QStringLiteral("Failed to load PDF: %1").arg(path));
    }
}

QString NativePdfDocument::renderPageDataUri(int pageIndex, int width) const {
    if (!m_loaded || pageIndex < 0 || pageIndex >= m_pageCount) return {};
    QImage img = m_renderer.renderPage(pageIndex, width);
    if (img.isNull()) return {};
    QByteArray pngData;
    QBuffer buf(&pngData);
    buf.open(QIODevice::WriteOnly);
    if (!img.save(&buf, "PNG")) return {};
    return QStringLiteral("data:image/png;base64,")
         + QString::fromLatin1(pngData.toBase64());
}
