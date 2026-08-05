// =============================================================================
// Translator.cpp — JSON-based i18n implementation
// =============================================================================
#include "Common/Utils/Translator.h"
#include "app/AppState.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QDebug>

Translator::Translator(QObject* parent)
    : QObject(parent)
{
}

void Translator::initialize(AppState* appState)
{
    m_appState = appState;

    if (!loadJson(QStringLiteral(":/translations.json"))) {
        qCritical() << "Translator: failed to load translations.json — "
                        "all lookups will return keys/English fallback";
        return;
    }

    // Read initial language
    m_lang = appState->languageIndex();

    // Keep m_lang synchronized with AppState → SettingsController changes
    connect(appState, &AppState::languageChanged, this, [this]() {
        if (m_appState) {
            setLanguage(m_appState->languageIndex());
        }
    });
}

// ── JSON loading ────────────────────────────────────────────────────────────

static QJsonArray parseLangArray(const QJsonValue& val)
{
    QJsonArray arr = val.toArray();
    // Ensure exactly kMaxLanguages entries; pad with empty strings if short.
    // If the JSON has more entries than the C++ code expects, truncate with
    // a warning so the mismatch is visible when a language is being added.
    if (arr.size() > Translator::kMaxLanguages) {
        qWarning() << "Translator: language array has" << arr.size()
                    << "entries, but kMaxLanguages is" << Translator::kMaxLanguages
                    << "— extra entries will be ignored";
        while (arr.size() > Translator::kMaxLanguages)
            arr.removeLast();
    }
    while (arr.size() < Translator::kMaxLanguages)
        arr.append(QString());
    return arr;
}

bool Translator::loadJson(const QString& path)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly)) {
        qCritical() << "Translator: cannot open" << path;
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qCritical() << "Translator: JSON parse error at offset"
                     << err.offset << ":" << err.errorString();
        return false;
    }

    QJsonObject root = doc.object();

    // ── Properties ──
    QJsonObject props = root.value("properties").toObject();
    for (auto it = props.begin(); it != props.end(); ++it) {
        m_strings[it.key()] = parseLangArray(it.value());
    }
    qDebug() << "Translator: loaded" << m_strings.size() << "property strings";

    // ── Group names ──
    QJsonArray groups = root.value("groupNames").toArray();
    for (const auto& g : groups)
        m_groupNames.append(parseLangArray(g));
    qDebug() << "Translator: loaded" << m_groupNames.size() << "group names";

    // ── Group prefix ──
    m_groupPrefix = parseLangArray(root.value("groupPrefix"));

    // ── Diagnostic names ──
    QJsonObject dn = root.value("diagName").toObject();
    for (auto it = dn.begin(); it != dn.end(); ++it) {
        bool ok;
        int id = it.key().toInt(&ok);
        if (ok)
            m_diagNames[id] = parseLangArray(it.value());
    }
    qDebug() << "Translator: loaded" << m_diagNames.size() << "diagName entries";

    // ── Diagnostic descriptions ──
    QJsonObject dd = root.value("diagDesc").toObject();
    for (auto it = dd.begin(); it != dd.end(); ++it) {
        bool ok;
        int id = it.key().toInt(&ok);
        if (ok)
            m_diagDescs[id] = parseLangArray(it.value());
    }
    qDebug() << "Translator: loaded" << m_diagDescs.size() << "diagDesc entries";

    // ── trMsg ──
    QJsonObject trmsg = root.value("trMsg").toObject();

    QJsonObject exact = trmsg.value("exact").toObject();
    for (auto it = exact.begin(); it != exact.end(); ++it)
        m_errorMessages[it.key()] = parseLangArray(it.value());
    qDebug() << "Translator: loaded" << m_errorMessages.size() << "trMsg exact entries";

    // JSON arrays store templates in order; the extraction script writes
    // them as a JSON array — check what we got
    QJsonValue tplVal = trmsg.value("templates");
    if (tplVal.isArray()) {
        QJsonArray tpls = tplVal.toArray();
        if (tpls.size() >= 1)
            m_tplUnsupportedProtocol = parseLangArray(tpls[0]);
        if (tpls.size() >= 2)
            m_tplPortRange = parseLangArray(tpls[1]);
    } else if (tplVal.isObject()) {
        // Named templates variant
        QJsonObject tplObj = tplVal.toObject();
        if (tplObj.contains("unsupported_protocol"))
            m_tplUnsupportedProtocol = parseLangArray(tplObj.value("unsupported_protocol"));
        if (tplObj.contains("port_range"))
            m_tplPortRange = parseLangArray(tplObj.value("port_range"));
    }
    qDebug() << "Translator: loaded trMsg templates";

    return true;
}

// ── Lookup helpers ──────────────────────────────────────────────────────────

QString Translator::select(const QJsonArray& arr) const
{
    return selectAt(arr, m_lang);
}

QString Translator::selectAt(const QJsonArray& arr, int langIdx)
{
    if (langIdx >= 0 && langIdx < arr.size()) {
        QString s = arr.at(langIdx).toString();
        if (!s.isEmpty())
            return s;
    }
    // Fallback to English
    if (kEnglishIndex < arr.size()) {
        QString en = arr.at(kEnglishIndex).toString();
        if (!en.isEmpty())
            return en;
    }
    return QString();
}

// ── QML API ─────────────────────────────────────────────────────────────────

QString Translator::tr(const QString& key) const
{
    auto it = m_strings.find(key);
    if (it != m_strings.end())
        return select(it.value());

    // Graceful degradation: return the key itself
    static QSet<QString> warned;
    if (!warned.contains(key)) {
        warned.insert(key);
        qWarning() << "Translator::tr: unknown key:" << key;
    }
    return key;
}

QString Translator::diagName(int id) const
{
    auto it = m_diagNames.find(id);
    return (it != m_diagNames.end()) ? select(it.value()) : QString();
}

QString Translator::diagDesc(int id) const
{
    auto it = m_diagDescs.find(id);
    return (it != m_diagDescs.end()) ? select(it.value()) : QString();
}

QString Translator::groupName(int idx) const
{
    if (idx < 0 || idx >= m_groupNames.size())
        return QString();
    return select(m_groupNames.at(idx));
}

QString Translator::groupPrefix(int idx) const
{
    return select(m_groupPrefix) + QString::number(idx + 1);
}

QString Translator::trMsg(const QString& en) const
{
    if (en.isEmpty())
        return en;

    // Exact match
    auto it = m_errorMessages.find(en);
    if (it != m_errorMessages.end())
        return select(it.value());

    // Parameterized: "Unsupported protocol: <scheme>:// — supported schemes: <list>"
    const QString unsupPrefix = QStringLiteral("Unsupported protocol: ");
    const QString unsupSep    = QStringLiteral(":// — supported schemes: ");
    if (en.startsWith(unsupPrefix) && !m_tplUnsupportedProtocol.isEmpty()) {
        int sepPos = en.indexOf(unsupSep, unsupPrefix.length());
        if (sepPos > 0) {
            QString scheme     = en.mid(unsupPrefix.length(), sepPos - unsupPrefix.length());
            QString schemeList = en.mid(sepPos + unsupSep.length());
            QString tpl = select(m_tplUnsupportedProtocol);
            tpl.replace(QStringLiteral("%1"), scheme);
            tpl.replace(QStringLiteral("%2"), schemeList);
            return tpl;
        }
    }

    // Parameterized: "Port must be between 1 and 65535 (got <n>)"
    const QString portPrefix = QStringLiteral("Port must be between 1 and 65535 (got ");
    if (en.startsWith(portPrefix) && en.endsWith(QLatin1Char(')'))
            && !m_tplPortRange.isEmpty()) {
        QString port = en.mid(portPrefix.length(), en.length() - portPrefix.length() - 1);
        QString tpl = select(m_tplPortRange);
        tpl.replace(QStringLiteral("%1"), port);
        return tpl;
    }

    // Fallback: return untranslated English
    return en;
}

// ── Language change ─────────────────────────────────────────────────────────

void Translator::setLanguage(int index)
{
    if (index < 0 || index >= kMaxLanguages || index == m_lang)
        return;
    m_lang = index;
    emit langChanged();
}
