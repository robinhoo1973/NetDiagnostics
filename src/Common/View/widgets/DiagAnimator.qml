import QtQuick
import NetDiagnostics.App 1.0
import "../theme/AnimationTokens.js" as Tokens

// ── DiagAnimator.qml — Living Diagnostics L4 animation dispatcher ──────
// Delegates `diagId → QRC animation URL` to C++ (AppState.diagAnimationUrl)
// so the enum→URL mapping lives in one place.  Uses Loader + source URL for
// platform-safe deferred loading (5WHY #4: inline Component → iOS crash).
//
// Usage: DiagAnimator { anchors.fill: parent; diagId: itemData.diagId; running: testRunning }

Item {
    id: root
    property int diagId: -1
    property bool running: false
    // 5WHY (review B5): lets JiggleAnimation rotate/scale the icon WELL
    // itself (real iOS-style icon jiggle) instead of a detached faint ring.
    // DiagBlock passes its icon-well Item; other animations ignore it.
    property var targetItem: null
    // 5WHY (复核 2026-08-21 用户诉求 "WiFi 信息动画同款弧线"): 动画锚点
    // 几何是母版图标事实——同一动画类型（WifiWave）用于不同图标（internet
    // 右下弧组 / wifi-info 同心弧）时需按图标分派。消费方（DiagBlock/hero）
    // 传入 iconName，装载时下发给动画（AnimationBase 统一声明该属性，
    // 无条件绑定——曾以 `"iconName" in item` 探测，已删）。
    property string iconName: ""
    // 5WHY (复核 2026-08-19 详情页重放): 有界播放属于动画层——消费方曾自带
    // Timer+硬编码 2400ms 窗口（与 AnimationTokens 周期脱钩、每消费方复制
    // 且 Jiggle 类动画因缺 targetItem 静默死掉）。bounded=true 时由窗口
    // 自动关闭 running，restart() 重放；瓦片等无界用法（bounded=false）
    // 行为不变。
    // 5WHY (复核 2026-08-19 重放可靠性): restart 经"先关后开"两帧实现——
    // Loader active=false 同步销毁旧实例（动画时钟归零），下一帧重建即为
    // 从头重放；直接 running=true 是同 URL 无重载的 no-op（同一动画类型
    // 的连续切换会静默失败）。关闭路径先停实例再停 Loader：顺序反了会
    // 销毁先于 onRunningChanged 清理（Jiggle 半程旋转残留）。
    property bool bounded: false
    // 5WHY (复核 2026-08-19): 窗口与各动画周期同为时序事实——从 tokens 读取
    // （replayWindowMs 单一来源），周期改版不再与窗口隐性脱钩。
    readonly property int windowMs: Tokens.tokens.replayWindowMs
    // 5WHY (复核 2026-08-19 收尾优雅): 窗口不是各循环周期的公倍数——硬截止
    // 会砍在半环（雷达波冻结在半扩散）。截止前 250ms 淡出：任何相位下
    // 收尾都平滑；restart 时回 1 淡入。
    property real _fadeOut: 1.0
    Behavior on _fadeOut { NumberAnimation { duration: 250 } }
    Timer {
        id: fadeTimer
        interval: Math.max(0, root.windowMs - 250)
        running: root.running && root.bounded
        onTriggered: root._fadeOut = 0
    }
    Timer {
        id: boundTimer
        interval: root.windowMs
        running: root.running && root.bounded
        onTriggered: root._stopInstance()
    }
    function _stopInstance() {
        // 5WHY (复核 2026-08-19 清理序): 命令式置假触发各动画的
        // onRunningChanged 清理（先于 Loader 销毁——顺序反了会销毁先于
        // 清理，Jiggle 半程旋转残留）。随后 root.running=false 令 Loader
        // active 归假销毁实例，重装 binding 无意义（旧舞蹈已简化）。
        if (loader.item) loader.item.running = false
        root.running = false
    }
    // 5WHY (复核 2026-08-19 无界路径清理): 瓦片等无界用法（bounded=false）
    // 不经过 _stopInstance——Loader.active 绑定先销毁实例，动画自身的
    // onRunningChanged 清理（Jiggle 旋转复位）被跳过，图标井冻结在 ±2.5°
    // 随机倾角直到下轮。属性变更处理器先于依赖绑定重估执行：此处先清
    // 实例再让其随 active 销毁。
    // 5WHY (复核 2026-08-19 绑定契约): bounded 用法必须不绑定 running——
    // _stopInstance/restart 命令式赋值会剥除消费方绑定（DiagBlock 绑定但
    // bounded=false 不经过；hero bounded 但不绑定）。bounded XOR 绑定。
    onRunningChanged: {
        if (!running && !root.bounded && loader.item) loader.item.running = false
    }
    function restart() {
        if (!root.bounded) return
        Qt.callLater(function() {
            // 5WHY (复核 2026-08-19 生命周期兜底): 详情页同帧 pop 时 callLater
            // 可能仍被派发——已销毁对象上赋值会抛 TypeError；守卫后静默跳过。
            if (!root) return
            root._fadeOut = 1
            root.running = true   // 计时器 running 绑定随此重新起算全窗（无需显式 restart）
        })
        root._stopInstance()
    }

    // 5WHY (复核 2026-08-19 作用域回归): 曾声明在 Loader 字面量内、active/
    // source 却经 root._loaderActive 引用——属性属于 Loader 而 root 是外层
    // Item，绑定求值 undefined → Loader 永不装载，全部运行动画与 hero 重放
    // 静默失效。根级声明（readonly 根级规则）供两者共用。
    readonly property bool _loaderActive: root.running && root.diagId >= 0
    Loader {
        id: loader
        anchors.fill: parent   // 5WHY (复核 2026-08-19 回归): 加 id 时曾误删——动画根几何全部由
                               // parent.width/height 推导，无此锚即 0×0 静默不可见
        opacity: root._fadeOut
        // 5WHY (复核 2026-08-19 单一条件): active 与 source 曾各自书写同一
        // 条件——新门控必须两处同步。_loaderActive 单一来源供两者共用。
        active: root._loaderActive
        // C++ resolves DiagId → animation URL — no QML-side switch needed
        source: root._loaderActive ? AppState.diagAnimationUrl(root.diagId) : ""
        onLoaded: {
            if (item) {
                item.running = Qt.binding(function() { return root.running })
                item.targetItem = Qt.binding(function() { return root.targetItem })
                // 5WHY (复核 2026-08-20 锚点管道删除): 锚点几何单一来源为
                // AnimationTokens.js（各动画 QML 默认直读本类型锚点）——
                // C++ 下发链（AppState.diagAnimationAnchor）曾是运行时
                // 再解析同文件后把相同值传回来（纯传递冗余），已删。
                // 5WHY (复核 2026-08-21): 同类型多图标仍需图标名分派
                // （WifiWave：internet vs wifi-info 两套锚点）——消费方
                // 提供 iconName，动画按名从 tokens 选锚点集。曾以
                // `"iconName" in item` 探测（版本敏感）——AnimationBase
                // 统一声明后无条件绑定，探测删除。
                item.iconName = Qt.binding(function() { return root.iconName })
            }
        }
        // 5WHY: onLoaded one-frame race — item.running is set AFTER
        // Component.onCompleted of the animation fires. If the animation
        // checks its own 'running' in onCompleted, it sees false and skips
        // init. Mitigation: animation components check running reactively
        // (via binding or onRunningChanged), not in onCompleted.
    }
}
