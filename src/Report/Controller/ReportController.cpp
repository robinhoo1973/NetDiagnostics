// =============================================================================
// ReportController.cpp — Report page controller
// =============================================================================
#include "Report/Controller/ReportController.h"
#include "app/AppState.h"
#include "Report/Model/ReportEngine.h"
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QFileDialog>
#endif

ReportController::ReportController(AppState* appState, QObject* parent)
    : QObject(parent), m_appState(appState) {}

QString ReportController::buildReportHtml(bool fullDetail, bool darkBackground) const { return m_appState->buildReportHtml(fullDetail, darkBackground); }
QString ReportController::buildRichHtmlDocument(bool darkBackground) const { return m_appState->buildRichHtmlDocument(darkBackground); }
QString ReportController::renderPreviewImage(const QString& html, int width) const { return m_appState->renderPreviewImage(html, width); }
QString ReportController::generatePreviewPdf() const { return m_appState->generatePreviewPdf(); }
QString ReportController::defaultReportPath(const QString& ext) const { return m_appState->defaultReportPath(ext); }
QString ReportController::exportHtml(const QString& filePath, bool darkBackground) const { return m_appState->exportHtml(filePath, darkBackground); }
QString ReportController::exportPdf(const QString& filePath) const { return m_appState->exportPdf(filePath); }
void ReportController::openPdfExternally() const { m_appState->openPdfExternally(); }
void ReportController::openHtmlExternally() const { m_appState->openHtmlExternally(); }
void ReportController::requestSavePath(const QString& format) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    auto* dlg = new QFileDialog(nullptr, QStringLiteral("Save Report"),
                                ReportEngine::defaultReportPath(format));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAcceptMode(QFileDialog::AcceptSave);
    dlg->setNameFilter(format == QLatin1String("pdf")
        ? QStringLiteral("PDF document (*.pdf)")
        : QStringLiteral("HTML document (*.html)"));
    dlg->setDefaultSuffix(format);
    connect(dlg, &QFileDialog::fileSelected, this, [this, format](const QString& p) {
        if (!p.isEmpty()) emit savePathPicked(format, p);
    });
    dlg->open();
#else
    emit savePathPicked(format, ReportEngine::defaultReportPath(format));
#endif
}
