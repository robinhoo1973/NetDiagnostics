// =============================================================================
// ResultProperty.h — Key-value pair with severity, supports tree nesting
// =============================================================================
#pragma once

#include <QString>
#include <QVector>

enum class ResultPropertySeverity {
    Info, Warning, Error
};

struct ResultProperty {
    QString label;
    QString value;
    ResultPropertySeverity severity = ResultPropertySeverity::Info;
    QVector<ResultProperty> children;

    ResultProperty() = default;
    ResultProperty(const QString& l, const QString& v,
                   ResultPropertySeverity s = ResultPropertySeverity::Info)
        : label(l), value(v), severity(s) {}
};
