// =============================================================================
// TranslationsProxy.qml — reactive translation bridge exposed to QML as "T"
//
// 5WHY root-cause analysis (language switch not reflected in the UI):
//   Why 1: switching the language in Settings left most UI text frozen in
//          the previous language; neither navigation nor any other action
//          produced a complete switch.
//   Why 2: QML bindings calling T.tr()/T.diagName()/... were never
//          invalidated when the language changed, so they kept returning
//          the stale string.
//   Why 3: the previous fix called QQmlProperty::read(this, "lang") inside
//          Translator::select(), assuming it registers a QML binding
//          dependency.  It does NOT — verified against Qt 6.8 source:
//          qqmlproperty.cpp contains ZERO references to QQmlPropertyCapture,
//          so the read was a plain value read with no binding invalidation.
//   Why 4: empirically (standalone tests), only property reads INSIDE the
//          binding's own expression are captured by the binding engine.
//          Reads inside QML function bodies ARE captured, but a function
//          that additionally references an external C++ context-property
//          object (TImpl) fails to resolve it during re-evaluation
//          (null / "not a function" / one-step lag) — so delegating to a
//          C++ object from inside the proxy is unreliable.
//   Why 5: the only reliable, portable pattern is a QML object whose
//          functions read a QML property (root.lang) plus self-contained
//          data — no external references.
//
// Fix: this proxy owns the translation data (loaded from translations.json
// via XMLHttpRequest on qrc:/, exactly the same JSON the C++ Translator
// reads) and exposes tr()/diagName()/diagDesc()/groupName()/groupPrefix()/
// trMsg().  Each function reads `root.lang` first, so — during binding
// evaluation — the QML binding engine captures (root, lang) as a dependency
// of the calling binding.  When appState.languageIndex changes, root.lang
// changes and every binding that called a proxy function re-evaluates with
// the new language.  Verified with standalone harnesses against the real
// translations.json (EN → 简体中文 → 日本語 all update correctly).
// =============================================================================
import QtQuick
import QtQml

QtObject {
    id: root

    // ── Language index constants — keep in sync with translations.json ──
    // ordering: 0=ZH_CN,1=ZH_TW,2=JA,3=KO,4=HI,5=VI,6=TR,7=EN,8=FR,9=DE,
    // 10=RU,11=IT,12=ES,13=PT,14=AR.
    readonly property int kLangEnglish: 7
    readonly property int kLangArabic: 14

    // Reactive language index — bound to AppState (NOTIFY languageChanged).
    // Every translation function reads root.lang so calling bindings
    // re-evaluate when this changes.
    property int lang: appState ? appState.languageIndex : root.kLangEnglish

    // ── Right-to-left (RTL) support ────────────────────────────────────
    // Languages whose script reads right-to-left.  Only Arabic (14) today;
    // adding Hebrew/Persian later is a one-line change here.  Screens enable
    // LayoutMirroring from this flag and use the alignment/elide helpers
    // below so full-width labels render from the correct edge.
    readonly property var _rtlLanguages: [root.kLangArabic]
    property bool isRtl: root._rtlLanguages.indexOf(root.lang) !== -1

    // "Start"/"End" text alignment & elide for the active script direction.
    // LTR: start=left, end=right, elide at right.
    // RTL: start=right, end=left, elide at left.
    property int textAlignStart: root.isRtl ? Text.AlignRight : Text.AlignLeft
    property int textAlignEnd:   root.isRtl ? Text.AlignLeft  : Text.AlignRight
    property int textElideStart: root.isRtl ? Text.ElideLeft  : Text.ElideRight

    // ── Translation tables (loaded from translations.json) ──────────────
    property var _props: ({})       // properties: key -> [15 langs]
    property var _propLabels: ({})  // propLabels:   EN label -> [15 langs]
    property var _groups: []        // groupNames:  [5 x [15 langs]]
    property var _groupPrefix: []   // groupPrefix: [15 langs]
    property var _diagNames: ({})   // diagName:    "id" -> [15 langs]
    property var _diagDescs: ({})   // diagDesc:    "id" -> [15 langs]
    property var _msgExact: ({})    // trMsg.exact: EN text -> [15 langs]
    property var _msgTpl: []        // trMsg.templates: [[15],[15]]
    property bool _loaded: false

    // ── Load translations.json ─────────────────────────────────────────
    // Sync XHR on qrc:/ is blocked in Qt 6.8 ("Invalid state"), so main.cpp
    // reads :/translations.json in C++ and exposes the raw JSON string as
    // the TJson context property.  We parse it synchronously here (JSON.parse
    // is a pure JS call — no external object resolution at binding time).
    function _load() {
        if (!TJson) return
        var j = JSON.parse(TJson)
        root._props      = j.properties  || {}
        root._propLabels = j.propLabels  || {}
        root._groups     = j.groupNames  || []
        root._groupPrefix = j.groupPrefix || []
        root._diagNames  = j.diagName    || {}
        root._diagDescs  = j.diagDesc    || {}
        var tm = j.trMsg || {}
        root._msgExact   = tm.exact      || {}
        root._msgTpl     = tm.templates  || []
        root._loaded = true
    }

    // Pick arr[lang], falling back to English, then "".
    function _pick(arr) {
        var a = arr || []
        if (a[root.lang] !== undefined && a[root.lang] !== "") return a[root.lang]
        var en = a[root.kLangEnglish]
        if (en !== undefined && en !== "") return en
        return ""
    }

    // ── QML API (API-parity with the former C++ Translator) ─────────────
    function tr(key) {
        let _ = root.lang // register binding dependency (captured by engine)
        if (!root._loaded) root._load()
        return root._pick(root._props[key]) || key
    }

    // Translate a diagnostic PROPERTY label (English, from C++) via the
    // propLabels table; falls back to the original English label when the
    // label isn't in the table (R4).
    function trProp(label) {
        let _ = root.lang // register binding dependency
        if (!root._loaded) root._load()
        if (!label) return label
        var arr = root._propLabels[label]
        if (!arr) return label
        var v = root._pick(arr)
        return v !== "" ? v : label
    }

    function diagName(id) {
        let _ = root.lang
        if (!root._loaded) root._load()
        return root._pick(root._diagNames["" + id])
    }

    function diagDesc(id) {
        let _ = root.lang
        if (!root._loaded) root._load()
        return root._pick(root._diagDescs["" + id])
    }

    function groupName(idx) {
        let _ = root.lang
        if (!root._loaded) root._load()
        if (idx < 0 || idx >= root._groups.length) return ""
        return root._pick(root._groups[idx])
    }

    function groupPrefix(idx) {
        let _ = root.lang
        if (!root._loaded) root._load()
        return root._pick(root._groupPrefix) + (idx + 1)
    }

    // Translate C++ error/validation messages (same logic as the C++
    // Translator): exact match + parameterized templates.
    function trMsg(en) {
        let _ = root.lang
        if (!root._loaded) root._load()
        if (!en) return en

        if (root._msgExact[en] !== undefined)
            return root._pick(root._msgExact[en])

        // "Unsupported protocol: <scheme>:// — supported schemes: <list>"
        var unsupPrefix = "Unsupported protocol: "
        var unsupSep    = ":// — supported schemes: "
        if (en.indexOf(unsupPrefix) === 0 && root._msgTpl[0]) {
            var sepPos = en.indexOf(unsupSep, unsupPrefix.length)
            if (sepPos > 0) {
                var scheme = en.substring(unsupPrefix.length, sepPos)
                var list   = en.substring(sepPos + unsupSep.length)
                var tpl = root._pick(root._msgTpl[0])
                return tpl.replace("%1", scheme).replace("%2", list)
            }
        }

        // "Port must be between 1 and 65535 (got <n>)"
        var portPrefix = "Port must be between 1 and 65535 (got "
        if (en.indexOf(portPrefix) === 0 && en.charAt(en.length - 1) === ")"
                && root._msgTpl[1]) {
            var port = en.substring(portPrefix.length, en.length - 1)
            return root._pick(root._msgTpl[1]).replace("%1", port)
        }

        return en
    }

    // Language setter — language flows through appState.setLanguage();
    // kept for API parity.
    function setLanguage(index) {
        if (appState) appState.setLanguage(index)
    }

    Component.onCompleted: root._load()
}
