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
    // 5WHY (2026-09-05 NOTIFY 契约破坏): 失败路径（文件缺失/加载失败）只
    // emit errorOccurred——loaded/pageCount 被重置为 false/0 却不发
    // loadedChanged/pageCountChanged。QML 绑定（导航栏 visible、页码标签
    // "1 / 5"）在成功→失败的重载后保持陈旧状态。任一结局均须发射
    // 变更通知（Qt 不因值未变而省略，重复发射无害）。
    emit loadedChanged();
    emit pageCountChanged();

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
