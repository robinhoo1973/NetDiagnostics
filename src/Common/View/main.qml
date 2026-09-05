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
        // （saveWindowMaximized 路径：直写重构后若启动 500ms 内即最大化，
        // geomRecorder 在 Maximized 态跳过、几何键可能从未写盘）
        // x/y=-1 哨兵进居中兜底——但 maximized 标志仍需恢复。旧条件（宽高
        // 默认值恒过）曾走恢复分支的 showMaximized，兜底分支不得丢标志。
        // 5WHY (复核 2026-09-05 五轮 双分支漂移): 曾两分支各自 showMaximized
        // 且守卫不对称（恢复分支无 g&&）——收敛为单一尾置，两路共用；
        // g&& 守卫承载移动端路径（桌面块跳过时 g 为未定义函数级 var）。
        if (g && g.maximized === true) win.showMaximized()
    }
    // P0-2：正常态几何持久化。5WHY (2026-09-05 /simplify 直写重构):
    // 曾为五件协作状态机（_lastNormalGeom/_transitioning/_prevVisibility/
    // geomRecorder/transitionWatchdog）在关闭时反推几何——每轮审查都在
    // 转场交错上发现新洞（最大化帧污染、退化坐标、看门狗误解锁……），且
    // 非正常退出（崩溃/强杀/断电——诊断应用自身的故障形态）丢光本会话
    // 全部几何（几何只在下一次 onClosing 才落盘）。改为稳定时直写：
    // geomRecorder 是唯一写者——500ms 防抖在 Windowed 态几何稳定后立即
    // 落盘（连续变更持续重启计时器，动画中间帧不会落盘；最大化态跳过，
    // 最大化帧永不当正常几何写入），onClosing 只冲刷 winMax 标志一行。
    // 直写语义：非正常退出至多丢最后 500ms；最大化关闭落 max 标志、几何键
    // 保持最近一次稳定正常态值；关窗不再读帧，转场中间帧污染类结构性消失。
    // 5WHY (simplify 二轮 2026-09-05 放置态替代坐标哨兵): 曾以 x==0&&y==0
    // 作"WM 未放置"哨兵——合法贴角 (0,0) 的窗口会被整会话静默停用持久化。
    // _placed 标记真实放置状态：可见后的任何几何事件即视为 WM 已放置；
    // 启动恢复的编程赋值（visible 之前）不算——首启未放置帧不落盘、
    // 恢复路径的冗余写回也随之消失（盘上即刚读回的值）。
    property bool _placed: false
    Timer {
        id: geomRecorder
        interval: 500
        repeat: false
        onTriggered: {
            if (ThemeEngine.isMobile || !win.visible || win.visibility !== Window.Windowed) return
            if (!win._placed) return
            AppState.saveWindowGeometry(win.x, win.y, win.width, win.height, false)
        }
    }
    // 5WHY (复核 2026-09-05 /simplify 移动端死功): 计时器重启在移动端
    // 100% 空转（onTriggered 首行即返回）——统一入口早退。
    function _pokeRecorder() {
        if (ThemeEngine.isMobile) return
        geomRecorder.restart()
    }
    onXChanged: { if (win.visible) win._placed = true; _pokeRecorder() }
    onYChanged: { if (win.visible) win._placed = true; _pokeRecorder() }
    onWidthChanged: _pokeRecorder()
    onHeightChanged: _pokeRecorder()
    onClosing: function(closeEvent) {
        // 直写重构后关窗只负责 max 标志：正常几何已由 geomRecorder 稳定时
        // 落盘（至多 500ms 滞后），最小化/还原转场等中间帧不再经关窗路径。
        if (ThemeEngine.isMobile) return
        AppState.saveWindowMaximized(visibility === Window.Maximized)
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
                // 直写重构后按钮无记录职责：几何在稳定时落盘（geomRecorder），
                // 重复点击 showMaximized/showNormal 幂等，无需过渡锁/看门狗。
                if (win.visibility === Window.Maximized) win.showNormal()
                else win.showMaximized()
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
