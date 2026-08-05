// =============================================================================
// Translator.h — JSON-based i18n singleton (replaces Translations.qml)
//
// Loads translations.json from QRC at startup.  Exposed to QML as context
// property "T".  Adding a language is a data change (one JSON key per entry)
// — no QML or C++ code change needed.
// =============================================================================
#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>
#include <QJsonArray>

class AppState;

class Translator : public QObject {
    Q_OBJECT

    // QML bindings read T.lang → tracked by binding engine.
    // When langChanged() fires, all T.tr() / T.diagName() / etc.
    // dependencies re-evaluate automatically.
    Q_PROPERTY(int lang READ lang NOTIFY langChanged)

public:
    // Language count and indexing constants — used by parseLangArray() and
    // by SettingsController for range checks.
    static constexpr int kEnglishIndex = 7;
    static constexpr int kMaxLanguages = 15;

    explicit Translator(QObject* parent = nullptr);

    // Must be called after AppState is available.
    // Loads translations.json and connects to language change signals.
    void initialize(AppState* appState);

    int lang() const { return m_lang; }

    // ── QML-invokable translation API ─────────────────────────────────

    // Primary key-based lookup.  Returns translated string for current
    // language, falls back to English (index 7), then to key itself.
    Q_INVOKABLE QString tr(const QString& key) const;

    // Diagnostic test names / descriptions (IDs 0-45).
    Q_INVOKABLE QString diagName(int id) const;
    Q_INVOKABLE QString diagDesc(int id) const;

    // Group labels (indices 0-4 → G1-G5).
    Q_INVOKABLE QString groupName(int idx) const;
    Q_INVOKABLE QString groupPrefix(int idx) const;

    // Translate C++ error/validation messages.
    // Exact-match dictionary + parameterized pattern handlers.
    Q_INVOKABLE QString trMsg(const QString& en) const;

public slots:
    void setLanguage(int index);

signals:
    void langChanged();

private:
    bool loadJson(const QString& path);

    // Select from a 15-element array: arr[lang] if non-empty, else arr[7] (EN).
    QString select(const QJsonArray& arr) const;
    static QString selectAt(const QJsonArray& arr, int langIdx);

    AppState* m_appState = nullptr;
    int m_lang = 7;  // English default

    // ── Translation tables ────────────────────────────────────────────

    // Property translations: key → 15-language array
    QHash<QString, QJsonArray> m_strings;

    // Group names: 5 entries, each a 15-language array
    QVector<QJsonArray> m_groupNames;

    // Group prefix "G": 15-language array
    QJsonArray m_groupPrefix;

    // Diagnostic names / descriptions: id → 15-language array
    QHash<int, QJsonArray> m_diagNames;
    QHash<int, QJsonArray> m_diagDescs;

    // trMsg exact-match dictionary: English text → 15-language array
    QHash<QString, QJsonArray> m_errorMessages;

    // trMsg parameterized templates
    QJsonArray m_tplUnsupportedProtocol;  // %1=scheme, %2=schemeList
    QJsonArray m_tplPortRange;            // %1=port
};
