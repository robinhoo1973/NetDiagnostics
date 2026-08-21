// =============================================================================
// AnimationBase.qml — 检测动画基类（10 动画共享）
//
// 5WHY (复核 2026-08-19 样板收敛): 10 个动画文件各自复制 running/targetItem
// 声明与 accent 色属性——且名漂移（accentColor/glowColor/arrowColor 三名，
// 统一绑定无从下手）。基类承载 running/targetItem/accentColor + 停止清理
// 钩子 resetVisuals()（子类覆盖）；子类只留绘制逻辑。
// =============================================================================
import QtQuick
import "../../theme" as T

Item {
    id: root

    property bool running: false
    // 统一声明：DiagAnimator 的 targetItem 绑定对全部动画类型一致生效
    //（Jiggle 以之旋转图标井，其余动画声明但不使用）。
    property var targetItem: null
    // 统一声明：DiagAnimator 的 iconName 绑定对全部动画类型一致生效——
    // 按图标名分派几何的动画（WifiWave 经 wifiWaveAnchorSets）读取；
    // 其余动画声明但不使用。5WHY (复核 2026-08-21 属性检测脆弱): 曾以
    // `"iconName" in item` 探测（版本敏感的 JS/QObject 互操作路径）——
    // 基类统一声明后无条件绑定，探测删除。
    property string iconName: ""
    // 5WHY (复核 2026-08-19 M3 令牌): 统一命名 accentColor（曾三名漂移）；
    // 动画仅经 DiagAnimator 在主题上下文内加载——直接绑定令牌，不再携带
    // 裸 "#60C8F8" 回退字面量（Palette.js 外复制即漂移）。个别动画以
    // 视觉决策覆盖（Converge 白箭头等）。
    property color accentColor: T.ThemeEngine.colors.primary
    // 停止清理钩子：子类覆盖（如 Check 复位盖片、Jiggle 复位旋转）。
    // 委托级动画（GeoLocate/WifiWave）在各自委托内以 RestartController
    // onStopped 复位（子类无法从 root 按 id 引用 Repeater 子项，覆写
    // 不可行——见 GeoLocate 5WHY）。
    function resetVisuals() {}
    onRunningChanged: if (!running) resetVisuals()
}
