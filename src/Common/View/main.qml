// main.qml — UI 壳：AppContent（dock 导航 + 四页面栈）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Window
import core
import theme
import widgets

Window {
    id: win
    width: 480
    height: 720
    minimumWidth: 360
    minimumHeight: 480
    // 5WHY (2026-08-22 UX-1): 曾 visible: true 在 onCompleted 几何恢复前
    // 首发——首帧 480×720 在左上角闪一下才跳到 1080×760 居中。先不可见，
    // 几何/主题恢复完毕后 onCompleted 末尾置真，首帧即最终形态。
    visible: false
    title: T.tr("appName")
    color: ThemeEngine.colors.surface
    // M5：桌面无边框 + 自定义窗口按钮；移动端保留原生 chrome（IME 兼容）
    flags: ThemeEngine.isMobile ? Qt.Window : Qt.FramelessWindowHint

    // 启动恢复：主题（UX 评审 3）+ 桌面窗口几何持久化（P0-2）+ 屏幕居中兜底
    Component.onCompleted: {
        if (AppState && ThemeEngine.mode !== AppState.themeMode)
            ThemeEngine.mode = AppState.themeMode
        if (!ThemeEngine.isMobile) {
            win.minimumWidth = 720
            win.minimumHeight = 560
            // P0-2：恢复上次几何；无存档（首启）走默认 1080×760 居中
            var g = AppState.restoreWindowGeometry()
            // 5WHY (复核 2026-09-05 三轮 多屏负坐标 + 首启居中失效): x/y 曾以
            // >= 0 拒绝负坐标——主屏左侧/上方的显示器合法坐标即负值（虚拟
            // 桌面坐标系），新落盘路径写入的负坐标被拒 → 窗口回主屏。且
            // C++ 缺键时返回 x/y=-1 哨兵（宽高却有 1080×760 默认值），
            // 旧条件恒过 → 首启从不走下方居中分支，窗口钉在 (0,0)。
            // 修正：-1 哨兵视为"无存档"整体走居中兜底；仅拒绝越界值。
            if (g.width >= win.minimumWidth && g.height >= win.minimumHeight
                && g.x !== -1 && g.y !== -1) {
                win.width = g.width
                win.height = g.height
                win.x = (g.x > -32000 && g.x < 32000) ? g.x : win.x
                win.y = (g.y > -32000 && g.y < 32000) ? g.y : win.y
            } else {
                win.width = 1080
                win.height = 760
                // 主窗口居中于所属屏幕
                var scr = win.screen
                if (scr) {
                    win.x = scr.virtualX + Math.round((scr.width - win.width) / 2)
                    win.y = scr.virtualY + Math.round((scr.height - win.height) / 2)
                }
            }
        }
        win.visible = true   // 几何恢复完成后首帧亮相（UX-1）
        // 5WHY (复核 2026-09-05 四轮 兜底丢标志): 仅 winMax 落盘的会话
        // （saveWindowMaximized 路径：手势最大化后 _lastNormalGeom 为空）
        // x/y=-1 哨兵进居中兜底——但 maximized 标志仍需恢复。旧条件（宽高
        // 默认值恒过）曾走恢复分支的 showMaximized，兜底分支不得丢标志。
        // 5WHY (复核 2026-09-05 五轮 双分支漂移): 曾两分支各自 showMaximized
        // 且守卫不对称（恢复分支无 g&&）——收敛为单一尾置，两路共用；
        // g&& 守卫承载移动端路径（桌面块跳过时 g 为未定义函数级 var）。
        if (g && g.maximized === true) win.showMaximized()
    }
    // P0-2：正常态几何随关闭落盘。5WHY (2026-09-05 最大化几何污染):
    // QML 在最大化态读 x/y/width/height 得到的是最大化帧——曾把它当
    // "还原尺寸"落盘（旧注释与事实相反），还原按钮永远回不到真实还原
    // 尺寸。最大化关闭只落 max 标志（saveWindowMaximized 不动几何键），
    // 已存的正常几何保持。
    // 5WHY (2026-09-05 复核 首会话几何丢失): 仅落 max 标志时，若本会话
    // 从未以正常态关闭过（用户调尺寸 → 最大化 → 关闭），几何键从未写盘，
    // 下次启动回默认 1080×760——用户选择的正常尺寸丢失。最大化按钮按下前
    // 记录当前正常几何到 _lastNormalGeom，最大化关闭时连同 max 标志落盘。
    // 5WHY (复核 2026-09-05 二轮 记录点收敛): 曾另挂 onVisibilityChanged
    // 在 Windowed 跃迁时记录——该记录点只能捕到 (a) 启动恢复的已落盘几何
    // （冗余）、(b) 首启 WM 尚未放置的 (0,0) 帧（下次启动钉死左上角）、
    // (c) 还原跃迁中 visibility 翻转先于几何回退时的最大化帧（污染正常
    // 几何）——三者无一负载，且 (b)(c) 会被最大化关闭分支持久化。
    // 5WHY (复核 2026-09-05 三轮 记录点补全): 按钮唯一记录点对 OS/WM 手势
    // 路径仍有洞——(a) 按钮最大化后还原、调尺寸、手势最大化关闭时
    // _lastNormalGeom 是过期值被落盘（还原不清记录）；(b) 首会话调尺寸后
    // 直接手势最大化关闭时记录为 null、几何键从未写盘。geomRecorder 以
    // 500ms 防抖在 Windowed 态几何稳定后刷新记录——连续变更持续重启计时
    // 器，动画中间帧/未放置首帧不会落记录；最小化/其他态关闭时 x/y 可能是
    // 退化帧（Windows 最小化窗口坐标为 -32000,-32000），关闭分支改落最近
    // 稳定正常几何，不污染几何键。按钮按下前的同步记录保留（防抖有 500ms
    // 滞后，按钮路径需即时值）。
    // _transitioning 过渡锁：最大化/还原的可视化翻转有平台延迟（还原时
    // visibility 先翻、几何后退），过渡期内点击按钮会拿到动画中间帧——
    // 锁由 geomRecorder 的几何稳定结算解除（还原方向），或由 Maximized
    // 翻转解除（最大化方向，该方向无记录点）；WM 拒绝过渡时由 watchdog
    // 兜底解锁，避免锁滞留。
    property var _lastNormalGeom: null
    property bool _transitioning: false
    // 5WHY (复核 2026-09-05 四轮 锁方向收敛): 曾无条件在每次 Windowed 跃迁
    // 加锁——首启 Hidden→Windowed 无几何后退，加锁只造成启动后 500ms 内
    // 最大化按钮静默失灵（geomRecorder 稳定结算才解锁）。仅最大化/最小化
    // 还原方向有中间帧风险（几何后退晚于 visibility 翻转），按来向加锁。
    property int _prevVisibility: Window.Hidden
    onVisibilityChanged: {
        if (visibility === Window.Maximized) {
            _transitioning = false
            // 5WHY (复核 2026-09-05 五轮 watchdog 方向): watchdog 只服务最大化
            // 方向（WM 拒绝时无翻转、geomRecorder 无事件不重启）——翻转达成即
            // 停表，避免残留计时器在随后 <1.5s 内的还原转场中误解锁（还原方向
            // 解锁由 geomRecorder 稳定结算负责；慢动画下 watchdog 1500ms 会
            // 提前解锁，重新打开中间帧入库通道）。
            transitionWatchdog.stop()
        } else if (visibility === Window.Windowed) {
            if (_prevVisibility === Window.Maximized || _prevVisibility === Window.Minimized)
                _transitioning = true
            geomRecorder.restart()   // 翻转本身可能不伴随几何事件——确保一次稳定结算
        }
        _prevVisibility = visibility
    }
    Timer {
        id: geomRecorder
        interval: 500
        repeat: false
        onTriggered: {
            if (ThemeEngine.isMobile || !win.visible || win.visibility !== Window.Windowed) return
            win._lastNormalGeom = Qt.rect(win.x, win.y, win.width, win.height)
            win._transitioning = false   // 几何稳定 = 过渡结束（解锁）
        }
    }
    Timer {
        id: transitionWatchdog
        interval: 1500
        repeat: false
        onTriggered: {
            // 最大化被 WM 拒绝时无任何状态翻转，锁会滞留——仍处 Windowed
            // 即视为过渡未发生，解锁恢复按钮可用。
            if (win.visibility === Window.Windowed) win._transitioning = false
        }
    }
    onXChanged: geomRecorder.restart()
    onYChanged: geomRecorder.restart()
    onWidthChanged: geomRecorder.restart()
    onHeightChanged: geomRecorder.restart()
    // 5WHY (复核 2026-09-05 五轮 三处同型调用): saveWindowGeometry(record…)
    // 曾三处逐字复制——签名扩展（如 DPI/屏幕字段）时漏改一处即静默落错。
    // 单一 helper 收敛。
    function saveNormalGeom(maximized) {
        AppState.saveWindowGeometry(_lastNormalGeom.x, _lastNormalGeom.y,
                                    _lastNormalGeom.width, _lastNormalGeom.height, maximized)
    }
    onClosing: function(closeEvent) {
        if (ThemeEngine.isMobile) return
        if (visibility === Window.Windowed) {
            // 5WHY (复核 2026-09-05 四轮 转场关闭): 还原动画中 visibility
            // 先翻、几何后退——过渡期内关闭会把最大化中间帧写成正常几何
            // （本 commit 要消除的污染类）。过渡中落最近稳定记录。
            // 5WHY (复核 2026-09-05 五轮 空记录守卫): 过渡中无稳定记录
            // （手势最大化后未结算即还原）时不再落当前中间帧——静默保留
            // 盘上最后有效几何，宁缺毋滥。
            if (_transitioning && _lastNormalGeom) saveNormalGeom(false)
            else if (!_transitioning)
                AppState.saveWindowGeometry(x, y, width, height, false)
        }
        else if (visibility === Window.Maximized) {
            if (_lastNormalGeom) saveNormalGeom(true)
            else AppState.saveWindowMaximized(true)
        } else if (_lastNormalGeom) {
            // 最小化/其他态：不读当前退化帧，落最近稳定正常几何
            saveNormalGeom(false)
        }
    }

    // H5：字体注册——JetBrains Mono（等宽）与 DejaVu Sans Mono（box-drawing/CJK
    // 合并回退）。不注册则所有等宽标签（瓦片计时/终端/图表刻度）回退到系统代换
    // 字体，终端表格对齐破碎。
    FontLoader { source: "fonts/JetBrainsMono-Regular.ttf" }
    FontLoader { source: "fonts/JetBrainsMono-Bold.ttf" }
    FontLoader { source: "fonts/DejaVuSansMono.ttf" }

    AppContent {
        id: appContent
        anchors.fill: parent
        onCloseRequested: win.close()
    }

    // ── 窗口按钮（右上角，仅桌面；UX 评审 7-6：38×38、8px 间距、8px 边距、
    //    悬停才显底色，关闭钮悬停红色——对齐 Windows/macOS 窗口控件惯例；
    //    8-3：页面浮层（详情/报告预览）打开时隐藏，避免按钮压住浮层）──
    Row {
        visible: !ThemeEngine.isMobile && !appContent.navBlocked
        anchors { top: parent.top; right: parent.right; topMargin: 5; rightMargin: 8 }
        spacing: 8

        WindowButton {
            iconName: "minimize"
            accName: T.tr("accMinimizeWindow")
            onClicked: win.showMinimized()
        }

        WindowButton {
            iconName: win.visibility === Window.Maximized ? "restore" : "maximize"
            accName: win.visibility === Window.Maximized ? T.tr("accRestoreWindow") : T.tr("accMaximizeWindow")
            onClicked: {
                if (win.visibility === Window.Maximized) {
                    // 5WHY (复核 2026-09-05 四轮 watchdog 方向): 还原方向解锁
                    // 由 geomRecorder 稳定结算兜底（onVisibilityChanged 必重启
                    // 计时器，即使无几何事件也会结算）——watchdog 的 1500ms
                    // 在慢动画下会提前解锁，允许点击把后退中间帧录进
                    // _lastNormalGeom。watchdog 只留给最大化方向（WM 拒绝时
                    // 无任何翻转，geomRecorder 无事件不重启）。
                    // 5WHY (复核 2026-09-05 五轮 单一写入点): 锁由
                    // onVisibilityChanged 的 Maximized→Windowed 跃迁布设
                    // （按钮与手势两条还原路径共用）——此处不再重复写入，
                    // WM 拒绝 showNormal 时（无翻转）锁也不会滞留。
                    win.showNormal()
                } else if (!win._transitioning) {
                    // 5WHY (2026-09-05 复核 首会话几何丢失): 最大化前把当前
                    // 正常几何存入 _lastNormalGeom——最大化关闭时连同 max
                    // 标志落盘，用户调过的尺寸不丢。
                    // 5WHY (复核 2026-09-05 三轮 过渡锁): 过渡期内重复点击
                    // 会把动画中间帧误录进 _lastNormalGeom（还原方向几何
                    // 后退晚于 visibility 翻转）——_transitioning 锁住过渡期
                    // （由 geomRecorder 几何稳定结算 / Maximized 翻转 /
                    // watchdog 三者任一解除），重复点击不重录/不重发。
                    win._lastNormalGeom = Qt.rect(win.x, win.y, win.width, win.height)
                    win._transitioning = true
                    transitionWatchdog.restart()
                    win.showMaximized()
                }
            }
        }

        WindowButton {
            iconName: "close"
            accName: T.tr("accCloseWindow")
            destructive: true
            onClicked: win.close()
        }
    }
}
