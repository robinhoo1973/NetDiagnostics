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
    visible: true
    title: T.tr("appName")
    color: ThemeEngine.colors.surface
    // M5：桌面无边框 + 自定义窗口按钮；移动端保留原生 chrome（IME 兼容）
    flags: ThemeEngine.isMobile ? Qt.Window : Qt.FramelessWindowHint

    // 启动恢复：主题（UX 评审 3）+ 桌面窗口尺寸（UX 评审 6）+ 屏幕居中（8-16）
    Component.onCompleted: {
        if (AppState && ThemeEngine.mode !== AppState.themeMode)
            ThemeEngine.mode = AppState.themeMode
        if (!ThemeEngine.isMobile) {
            win.width = 1080
            win.height = 760
            win.minimumWidth = 720
            win.minimumHeight = 560
            // 主窗口居中于所属屏幕
            var scr = win.screen
            if (scr) {
                win.x = scr.virtualX + Math.round((scr.width - win.width) / 2)
                win.y = scr.virtualY + Math.round((scr.height - win.height) / 2)
            }
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

        Rectangle {
            width: 38; height: 38; radius: 6
            color: minArea.containsMouse ? Qt.alpha(ThemeEngine.colors.borderCard, 0.6) : "transparent"
            AppIcon {
                anchors.centerIn: parent
                name: "minimize"; size: 14
                color: minArea.containsMouse ? ThemeEngine.colors.textPrimary : ThemeEngine.colors.textSecondary
            }
            MouseArea {
                id: minArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: win.showMinimized()
            }
            Accessible.name: T.tr("accMinimizeWindow")
            Accessible.role: Accessible.Button
        }

        Rectangle {
            width: 38; height: 38; radius: 6
            color: maxArea.containsMouse ? Qt.alpha(ThemeEngine.colors.borderCard, 0.6) : "transparent"
            AppIcon {
                anchors.centerIn: parent
                name: win.visibility === Window.Maximized ? "restore" : "maximize"
                size: 14
                color: maxArea.containsMouse ? ThemeEngine.colors.textPrimary : ThemeEngine.colors.textSecondary
            }
            MouseArea {
                id: maxArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: {
                    if (win.visibility === Window.Maximized) win.showNormal()
                    else win.showMaximized()
                }
            }
            Accessible.name: win.visibility === Window.Maximized ? T.tr("accRestoreWindow") : T.tr("accMaximizeWindow")
            Accessible.role: Accessible.Button
        }

        Rectangle {
            width: 38; height: 38; radius: 6
            color: closeArea.containsMouse ? ThemeEngine.colors.failRed : "transparent"
            AppIcon {
                anchors.centerIn: parent
                name: "close"; size: 14
                color: closeArea.containsMouse ? "#FFFFFF" : ThemeEngine.colors.textSecondary
            }
            MouseArea {
                id: closeArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: win.close()
            }
            Accessible.name: T.tr("accCloseWindow")
            Accessible.role: Accessible.Button
        }
    }
}
