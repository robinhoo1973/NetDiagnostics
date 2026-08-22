// =============================================================================
// AppIcon.qml — 运行时精确着色图标（图标管线 v4，方案 B：单母版 + C++ 着色）
//
// 协议: image://icon/<colorHex>/<name>?theme=dark|light
//   颜色在 C++ IconProvider 内精确替换（无 41 色烘焙、无最近似匹配）。
//   尺寸/高 DPI 由 Qt 引擎按 sourceSize×窗口 dpr 传入 provider（官方契约），
//   QML 侧不再自报 dpr。
//
// 切主题防串图（历史 5WHY 六重防线之 URL 失效键）：
//   source 绑定显式读取 ThemeEngine.isDark —— 主题切换时绑定重算 → URL 变化
//   （theme=dark↔light）→ Qt 图缓存与 provider 渲染缓存同时 miss → 强制重渲染。
// =============================================================================
import QtQuick
import theme

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20
    property bool mirror: false
    // 同步加载 opt-in（默认关）：常驻首屏小字形（tab 徽标 16px 等）异步解码会
    // 让底座先于字形渲染一帧（pop-in，用户感知为"图标位置怪"）。本地 provider
    // 解码 16px 成本可忽略；大图/长列表勿开。
    property bool syncLoad: false

    width: size; height: size
    implicitWidth: size; implicitHeight: size
    visible: name !== ""

    // color → 6 位大写十六进制（颜色通道转精确请求色）
    function _hexOf(c) {
        function h(n) {
            var s = Math.round(Math.max(0, Math.min(1, n)) * 255).toString(16)
            return s.length < 2 ? "0" + s : s
        }
        return (h(c.r) + h(c.g) + h(c.b)).toUpperCase()
    }

    readonly property string _request: {
        // 显式读取 ThemeEngine.isDark → 主题切换时本绑定重算 → URL 变化
        var theme = ThemeEngine.isDark ? "dark" : "light"
        return root.name !== ""
            ? ("image://icon/" + _hexOf(root.color) + "/" + root.name
               + "?theme=" + theme)
            : ""
    }

    transform: Scale {
        origin.x: width / 2
        xScale: mirror ? -1 : 1
    }

    Image {
        anchors.fill: parent
        source: root._request
        // 按显示分辨率请求（sourceSize 即请求尺寸，高 DPI 由 Qt 引擎换算）
        sourceSize: Qt.size(root.size, root.size)
        fillMode: Image.PreserveAspectFit
        opacity: root.color.a
        cache: true
        asynchronous: !root.syncLoad
    }
}
