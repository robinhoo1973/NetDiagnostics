// =============================================================================
// TargetRedaction.h — safe display form for diagnostic targets
// =============================================================================
#pragma once

#include <QString>

namespace TargetRedaction {

// Removes credentials, query parameters, and fragments while retaining the
// scheme, host, port, and path needed to identify a diagnostic target.
inline QString forDisplay(const QString& target) {
    if (target.isEmpty()) return QStringLiteral("(none)");

    // 5WHY: URL parsing can reject an invalid user entry, but diagnostic logs,
    // reports, and error text must never fall back to exposing
    // `user:password@host` or query-string tokens. Redact structurally first,
    // independent of URL validity and platform-specific QUrl behavior.
    QString display = target;
    const int schemeEnd = display.indexOf(QStringLiteral("://"));
    const int authorityStart = schemeEnd < 0 ? 0 : schemeEnd + 3;
    int authorityEnd = display.size();
    const int pathStart = display.indexOf(QLatin1Char('/'), authorityStart);
    const int queryStart = display.indexOf(QLatin1Char('?'), authorityStart);
    const int fragmentStart = display.indexOf(QLatin1Char('#'), authorityStart);
    if (pathStart >= 0 && pathStart < authorityEnd) authorityEnd = pathStart;
    if (queryStart >= 0 && queryStart < authorityEnd) authorityEnd = queryStart;
    if (fragmentStart >= 0 && fragmentStart < authorityEnd) authorityEnd = fragmentStart;

    const int userInfoEnd = display.lastIndexOf(QLatin1Char('@'), authorityEnd - 1);
    if (userInfoEnd >= authorityStart)
        display.remove(authorityStart, userInfoEnd - authorityStart + 1);

    // Query strings and fragments often carry API keys, signed URLs, or
    // client-side session tokens. Omit both from every display/export path.
    int sensitiveSuffix = display.indexOf(QLatin1Char('?'));
    const int displayFragmentStart = display.indexOf(QLatin1Char('#'));
    if (sensitiveSuffix < 0
        || (displayFragmentStart >= 0 && displayFragmentStart < sensitiveSuffix)) {
        sensitiveSuffix = displayFragmentStart;
    }
    if (sensitiveSuffix >= 0) display.truncate(sensitiveSuffix);

    return display;
}

} // namespace TargetRedaction
