// =============================================================================
// ResultProperty.h — Key-value pair with severity, supports tree nesting
// =============================================================================
#pragma once

#include <QString>
#include <QVector>

enum class ResultPropertySeverity {
    Info, Warning, Error
};

// 5WHY (2026-09-27 描述符同门): 严重度 token 曾以手写三元映射（AppState
// resultFor）——与 statusDescriptor 表驱动机制双轨，未来加枚举值三元静默
// 塌缩为 info。描述符表单一事实源，QML 只比对稳定名。
struct ResultPropertySeverityDescriptor {
    ResultPropertySeverity severity;
    const char* token;   // "info" / "warning" / "error"
};

inline const char* severityToken(ResultPropertySeverity s) {
    static const ResultPropertySeverityDescriptor table[] = {
        { ResultPropertySeverity::Info,    "info" },
        { ResultPropertySeverity::Warning, "warning" },
        { ResultPropertySeverity::Error,   "error" },
    };
    for (const auto& d : table)
        if (d.severity == s) return d.token;
    return "info";
}

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
