// =============================================================================
// PlatformShare.h — OS "share sheet" abstraction (mobile only)
// =============================================================================
// Presents the native share UI for a file:
//   iOS     — UIActivityViewController
//   Android — Intent.ACTION_SEND via a FileProvider content:// URI
// Not defined on desktop (desktop hands off to the mail client in AppState).
#pragma once
#include <QString>

void platformShareFile(const QString& filePath, const QString& mimeType, const QString& subject);
