// =============================================================================
// NativePdfDocument.h — QML-friendly wrapper around PlatformPdfRenderer
// =============================================================================
// Exposes PDF page rendering to QML via QObject properties + invokable methods.
// Used on iOS/Android where QtPdf is unavailable. Desktop uses QtPdf directly.
#pragma once
#include <QObject>
#include <QImage>
#include <QUrl>
#include "platform/PlatformPdfRenderer.h"

class NativePdfDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
public:
    explicit NativePdfDocument(QObject* parent = nullptr);
    ~NativePdfDocument() override;

    QUrl source() const { return m_source; }
    void setSource(const QUrl& url);
    int pageCount() const { return m_pageCount; }
    bool isLoaded() const { return m_loaded; }

    // Render a page (0-indexed) to a PNG base64 data URI for QML Image.
    // Returns empty string on failure.
    Q_INVOKABLE QString renderPageDataUri(int pageIndex, int width = 800) const;

signals:
    void sourceChanged();
    void pageCountChanged();
    void loadedChanged();
    void errorOccurred(const QString& message);

private:
    PlatformPdfRenderer m_renderer;
    QUrl m_source;
    int m_pageCount = 0;
    bool m_loaded = false;
};
