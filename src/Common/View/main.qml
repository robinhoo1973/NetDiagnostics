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
            if (g.width >= win.minimumWidth && g.height >= win.minimumHeight) {
                win.width = g.width
                win.height = g.height
                win.x = (g.x >= 0 && g.x < 32000) ? g.x : win.x
                win.y = (g.y >= 0 && g.y < 32000) ? g.y : win.y
                win.visible = true   // 几何就绪后先亮相，最大化随后应用
                if (g.maximized === true) win.showMaximized()
                return
            }
            win.width = 1080
            win.height = 760
            // 主窗口居中于所属屏幕
            var scr = win.screen
            if (scr) {
                win.x = scr.virtualX + Math.round((scr.width - win.width) / 2)
                win.y = scr.virtualY + Math.round((scr.height - win.height) / 2)
            }
        }
        win.visible = true   // 几何恢复完成后首帧亮相（UX-1）
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
    // 几何）——三者无一负载，且 (b)(c) 会被最大化关闭分支持久化。记录点
    // 收敛为唯一：最大化按钮按下前（应用内唯一最大化入口）。OS/WM 手势
    // 最大化不更新记录，关闭时走 saveWindowMaximized fallback（只落 max
    // 标志、几何键不动，与旧行为等价）。
    property var _lastNormalGeom: null
    onClosing: function(closeEvent) {
        if (ThemeEngine.isMobile) return
        if (visibility !== Window.Maximized)
            AppState.saveWindowGeometry(x, y, width, height, false)
        else if (_lastNormalGeom)
            AppState.saveWindowGeometry(_lastNormalGeom.x, _lastNormalGeom.y,
                                        _lastNormalGeom.width, _lastNormalGeom.height, true)
        else
            AppState.saveWindowMaximized(true)
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
                if (win.visibility === Window.Maximized) win.showNormal()
                else {
                    // 5WHY (2026-09-05 复核 首会话几何丢失): 最大化前把当前
                    // 正常几何存入 _lastNormalGeom——最大化关闭时连同 max
                    // 标志落盘，用户调过的尺寸不丢。
                    win._lastNormalGeom = Qt.rect(win.x, win.y, win.width, win.height)
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
