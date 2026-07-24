// =============================================================================
// PlatformCapture.cpp — Desktop screenshot implementation (QScreen)
// =============================================================================
#include "Common/Platform/PlatformCapture.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>

bool platformCaptureScreenshot(const QString& filePath) {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return false;
    }

    QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        return false;
    }

    // Ensure parent directory exists
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    return pixmap.save(filePath, "PNG");
}
