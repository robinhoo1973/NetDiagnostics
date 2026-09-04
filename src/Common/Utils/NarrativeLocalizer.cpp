// =============================================================================
// NarrativeLocalizer.cpp — 报告/剪贴板叙述本地化（与 QML TranslationsProxy 同表）
// =============================================================================
#include "Common/Utils/NarrativeLocalizer.h"

#include "Common/Model/DiagnosticResult.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>

namespace {

// 5WHY (SIOF): 静态表放函数内惰性构造——进程启动期无跨 TU 依赖。
struct NarrTable {
    QHash<QString, QStringList> narratives;
    bool loaded = false;
};

NarrTable& narrTable() {
    static NarrTable t;
    return t;
}

QMutex& narrMutex() {
    static QMutex m;
    return m;
}

void ensureLoaded() {
    NarrTable& t = narrTable();
    {
        QMutexLocker lock(&narrMutex());
        if (t.loaded) return;
        QFile f(QStringLiteral(":/translations.json"));
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            const QJsonObject narrs = doc.object()
                .value(QStringLiteral("trMsg")).toObject()
                .value(QStringLiteral("narratives")).toObject();
            for (auto it = narrs.constBegin(); it != narrs.constEnd(); ++it) {
                const QJsonArray arr = it.value().toArray();
                QStringList langs;
                langs.reserve(arr.size());
                for (const QJsonValue& v : arr) langs.append(v.toString());
                t.narratives[it.key()] = langs;
            }
        }
        t.loaded = true;
    }
}

QString pickLang(const QStringList& langs, int langIndex) {
    if (langs.isEmpty()) return QString();
    if (langIndex >= 0 && langIndex < langs.size() && !langs.at(langIndex).isEmpty())
        return langs.at(langIndex);
    if (langs.size() > 7 && !langs.at(7).isEmpty())   // 7 = English（translations.json 约定）
        return langs.at(7);
    return QString();
}

} // namespace

QString NarrativeLocalizer::localized(const QString& key, const QVariantList& args, int langIndex) {
    if (key.isEmpty()) return QString();
    ensureLoaded();
    const QStringList langs = narrTable().narratives.value(key);
    const QString tpl = pickLang(langs, langIndex);
    if (tpl.isEmpty()) return QString();
    // 5WHY (C1 template injection): 分轮 replace 时，早先插入的参数值若含
    // "%N" 会被后续迭代当作占位符再次替换——攻击者可通过诊断输出注入
    // 模板占位符篡改报告内容。倒序替换只挡一半（args[i] 注入 %j 且 j≤i
    // 时仍被替换）。修复：单遍扫描模板——只在模板原文中识别 %N 占位符，
    // 参数值原样拼接，插入文本不再参与占位符解析（业界标准做法）。
    QString out;
    out.reserve(tpl.size() + args.size() * 8);
    for (int i = 0; i < tpl.size();) {
        if (tpl.at(i) == QLatin1Char('%') && i + 1 < tpl.size()
            && tpl.at(i + 1).isDigit()) {
            // 5WHY (2026-09-04 修正复核): 只读一位数字会误解析 %10——模板
            // nDnsIntegrity（translations.json 6 语言）真实使用 %10（完整性
            // 评分），单数字解析渲染成 args[0]+"0"（如"70/100"）且评分参数
            // 永不插入。按"最长合法索引"解析至多两位：%10 → 第 10 参；
            // %12 且仅 10 参 → 第 1 参 + 字面 '2'（与原实现的 %1 前缀
            // 替换语义一致）。
            const int d1 = tpl.at(i + 1).digitValue();
            int n = d1, len = 1;
            if (d1 >= 1 && i + 2 < tpl.size() && tpl.at(i + 2).isDigit()) {
                const int two = d1 * 10 + tpl.at(i + 2).digitValue();
                if (two >= 1 && two <= args.size()) { n = two; len = 2; }
            }
            if (n >= 1 && n <= args.size()) {
                out += args.at(n - 1).toString();
                i += 1 + len;
                continue;
            }
            // 越界占位符：保持原文（与原实现一致——无对应参数不替换）
        }
        out += tpl.at(i);
        ++i;
    }
    return out;
}

QString NarrativeLocalizer::forResult(const DiagnosticResult& r, int langIndex) {
    const QString key = r.data.value(QStringLiteral("narrativeKey")).toString();
    if (key.isEmpty()) return QString();
    return localized(key, r.data.value(QStringLiteral("narrativeArgs")).toList(), langIndex);
}
