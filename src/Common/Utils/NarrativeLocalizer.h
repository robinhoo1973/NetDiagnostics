// =============================================================================
// NarrativeLocalizer.h — C++ 侧叙述本地化（报告/剪贴板与详情页同源）
//
// 5WHY (2026-08-23 报告文本同步): 叙述多语言模板（trMsg.narratives）此前仅
// QML TranslationsProxy 消费——HTML/PDF 报告与剪贴板走 C++ 路径直接取
// DiagnosticResult.narrative（EN），与详情页呈现的本地化文本分叉。
// 本工具读同一 :/translations.json 表（Meyer's 单例惰性加载 + QMutex），
// 按语言索引格式化 %1..%9 占位；未命中返回空串（调用方回退 EN 原文）。
// =============================================================================
#pragma once

#include <QString>
#include <QVariantList>

class DiagnosticResult;

namespace NarrativeLocalizer {

// key+args → 当前语言模板文本；键缺失/该语言未译/空模板 → 空串。
QString localized(const QString& key, const QVariantList& args, int langIndex);

// 便捷入口：读 result.data 的 narrativeKey/narrativeArgs。
QString forResult(const DiagnosticResult& r, int langIndex);

} // namespace NarrativeLocalizer
