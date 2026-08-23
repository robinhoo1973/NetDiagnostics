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
    QString out = tpl;
    for (int i = 0; i < args.size(); ++i) {
        const QString v = args.at(i).toString();
        out.replace(QStringLiteral("%%1").arg(i + 1), v);
    }
    return out;
}

QString NarrativeLocalizer::forResult(const DiagnosticResult& r, int langIndex) {
    const QString key = r.data.value(QStringLiteral("narrativeKey")).toString();
    if (key.isEmpty()) return QString();
    return localized(key, r.data.value(QStringLiteral("narrativeArgs")).toList(), langIndex);
}
