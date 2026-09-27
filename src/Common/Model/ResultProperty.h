// =============================================================================
// ResultProperty.h — Key-value pair with severity, supports tree nesting
// =============================================================================
#pragma once

#include <QString>
#include <iterator>   // std::size（static_assert 穷尽性，5WHY v5）
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
    // 5WHY (2026-09-27 v5 编译期穷尽): 枚举与表解同步曾只在运行时以错误着色
    // 暴露——加枚举值漏表条目即编译失败（std::size 于 C++17）。
    static_assert(std::size(table) == 3,
                  "severityDescriptor table must cover every ResultPropertySeverity value");
    for (const auto& d : table)
        if (d.severity == s) return d.token;
    // 5WHY (2026-09-27 复核): 未知枚举值曾回退 "info"——与描述符表成立初衷
    // （新值不得静默塌缩为 info）自相矛盾。fail-visible：未知严重度按 error
    // 呈现，新值漏加表项即视觉可辨（评审抓获）。
    return "error";
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
