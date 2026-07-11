// =============================================================================
// ReportController.h — Report page controller
//
// Owns: report generation delegation (calls AppState for HTML/PDF generation)
// Accesses: AppState for report data and export functionality
// =============================================================================
#pragma once

#include <QObject>
#include <QString>

class AppState;

class ReportController : public QObject {
    Q_OBJECT

public:
    explicit ReportController(AppState* appState, QObject* parent = nullptr);

    Q_INVOKABLE QString buildReportHtml(bool fullDetail, bool darkBackground = false) const;
    Q_INVOKABLE QString buildRichHtmlDocument(bool darkBackground = true) const;
    Q_INVOKABLE QString renderPreviewImage(const QString& html, int width) const;
    Q_INVOKABLE QString generatePreviewPdf() const;
    Q_INVOKABLE QString defaultReportPath(const QString& ext) const;
    Q_INVOKABLE QString exportHtml(const QString& filePath, bool darkBackground = true) const;
    Q_INVOKABLE QString exportPdf(const QString& filePath) const;
    Q_INVOKABLE void openPdfExternally() const;
    Q_INVOKABLE void openHtmlExternally() const;
    Q_INVOKABLE void requestSavePath(const QString& format);

signals:
    void savePathPicked(const QString& format, const QString& path);

private:
    AppState* m_appState;
};
