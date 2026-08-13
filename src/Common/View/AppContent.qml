// =============================================================================
// AppContent.qml — 底部 dock 导航 + 页面栈 + 触摸滑动（重建版）
//
// 从原版 AppContent 迁入（StackView 方向动画 / 触摸滑动 / M3 dock），
// 导入改为模块风格；overlay 关闭走页面级 closeOverlay() 钩子。
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Diagnostics.View
import Dashboard.View
import Settings.View
import Configuration.View
import widgets
import theme

Item {
    id: content
    objectName: "appContent"
    readonly property alias stackView: stackView
    property bool compact: ThemeEngine.isMobile

    // navBlocked：页面浮层开启时，导航点击/滑动先关闭浮层
    readonly property bool navBlocked: stackView.currentItem
                                       && stackView.currentItem.overlayVisible === true
    signal closeRequested()

    function closeCurrentOverlay() {
        var item = stackView.currentItem
        if (!item) return
        if (typeof item.closeOverlay === "function") item.closeOverlay()
        else if (typeof item._detailVisible !== "undefined") item._detailVisible = false
    }

    // 5WHY（nav 动画方向）：push/pop 固定方向导致 dashboard↔diag 反向滑动。
    // _navDir 由目标/当前 index 决定，push 与 pop 同侧进入。
    property int _navDir: 1

    readonly property var tabScreens: ["dashboard", "diagnostic", "config", "settings"]
    readonly property var tabComponents: [dashboardComp, diagnosticComp, configComp, settingsComp]
    readonly property var tabLabels: [T.tr("dashboard"), T.tr("diagnostics"),
                                      T.tr("config"), T.tr("settings")]

    function switchToTab(idx) {
        if (idx < 0 || idx >= tabScreens.length) return
        var curIdx = currentTabIndex()
        if (idx > curIdx) _navDir = 1
        else if (idx < curIdx) _navDir = -1
        for (var i = 0; i < stackView.depth; i++) {
            var item = stackView.get(i)
            if (item && item.objectName === tabScreens[idx]) {
                stackView.pop(item)
                return
            }
        }
        var created = tabComponents[idx].createObject(stackView)
        if (typeof created.sectionAction !== "undefined")
            created.sectionAction.connect(handlePageAction)
        stackView.push(created)
    }

    // 页面 action 路由（UI-10）：openDetail 推入 DetailPage；back 弹出
    function handlePageAction(scope, action, payload) {
        if (action === "openDetail" && payload && payload.diagId !== undefined) {
            var d = detailComp.createObject(stackView)
            if (typeof d.sectionAction !== "undefined")
                d.sectionAction.connect(handlePageAction)
            d.detail = AppState.resultFor(payload.diagId)
            stackView.push(d)
        } else if (action === "back") {
            if (stackView.currentItem && stackView.currentItem.objectName === "detail")
                stackView.pop()
        }
    }

    function currentTabIndex() {
        var cur = stackView.currentItem
        if (!cur) return -1
        for (var i = 0; i < tabScreens.length; i++) {
            if (tabScreens[i] === cur.objectName) return i
        }
        return -1
    }

    Component { id: diagnosticComp; DiagnosticScreen { objectName: "diagnostic" } }
    Component { id: dashboardComp;  DashboardScreen  { objectName: "dashboard"  } }
    Component { id: configComp;     ConfigScreen     { objectName: "config"     } }
    Component { id: settingsComp;   SettingsScreen   { objectName: "settings"   } }
    Component { id: detailComp;     DetailPage       { objectName: "detail"     } }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── 页面栈 ──
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackView {
                id: stackView
                anchors.fill: parent
                clip: true
                initialItem: diagnosticComp

                pushEnter: Transition {
                    XAnimator { from: stackView.width * content._navDir; to: 0; duration: 220; easing.type: Easing.OutCubic }
                }
                pushExit: Transition {
                    XAnimator { from: 0; to: -stackView.width * content._navDir; duration: 220; easing.type: Easing.InCubic }
                }
                popEnter: Transition {
                    XAnimator { from: stackView.width * content._navDir; to: 0; duration: 220; easing.type: Easing.OutCubic }
                }
                popExit: Transition {
                    XAnimator { from: 0; to: -stackView.width * content._navDir; duration: 220; easing.type: Easing.InCubic }
                }
            }

            // ── 触摸滑动翻页（仅触屏、仅横向）──
            Item {
                anchors.fill: parent
                DragHandler {
                    id: pageSwipe
                    acceptedDevices: PointerDevice.TouchScreen
                    target: null
                    dragThreshold: 24
                    xAxis.enabled: true
                    yAxis.enabled: false
                    grabPermissions: PointerHandler.CanTakeOverFromItems
                                   | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                    property int swipeStartIndex: -1
                    property real swipeStartX: 0
                    property real swipeStartY: 0
                    property real swipeLastX: 0
                    property real swipeLastY: 0
                    onActiveChanged: {
                        if (active) {
                            swipeStartIndex = content.currentTabIndex()
                            swipeStartX = centroid.position.x
                            swipeStartY = centroid.position.y
                            swipeLastX = swipeStartX
                            swipeLastY = swipeStartY
                        } else if (swipeStartIndex >= 0) {
                            var dx = swipeLastX - swipeStartX
                            var dy = swipeLastY - swipeStartY
                            if (Math.abs(dx) >= 60 && Math.abs(dx) >= 1.5 * Math.abs(dy)) {
                                if (content.navBlocked) {
                                    content.closeCurrentOverlay()
                                } else if (dx < 0) {
                                    content.switchToTab(swipeStartIndex + 1)   // 左滑 → 下一个
                                } else {
                                    content.switchToTab(swipeStartIndex - 1)   // 右滑 → 上一个
                                }
                            }
                            swipeStartIndex = -1
                        }
                    }
                    onCentroidChanged: {
                        if (active) {
                            swipeLastX = centroid.position.x
                            swipeLastY = centroid.position.y
                        }
                    }
                }
            }
        }

        // ── 底部 dock（M3；RTL 下顺序保持）──
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: compact ? 48 : 56
            color: ThemeEngine.colors.navBar
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: false

            // 无边框窗口拖动（保留原语义）
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPositionChanged: function(mouse) {
                    if (mouse.buttons & Qt.LeftButton) {
                        var win = content.Window.window
                        if (win && typeof win.startSystemMove === "function")
                            win.startSystemMove()
                    }
                }
            }
            RowLayout {
                anchors {
                    fill: parent
                    leftMargin: compact ? 0 : 16
                    rightMargin: compact ? 4 : 16
                }
                Item { Layout.fillWidth: true }
                Row {
                    spacing: 4
                    Repeater {
                        model: [
                            { screen: "dashboard",  icon: "dashboard" },
                            { screen: "diagnostic", icon: "diagnostics" },
                            { screen: "config",     icon: "config" },
                            { screen: "settings",   icon: "gear" }
                        ]
                        delegate: ItemDelegate {
                            id: navBtn
                            property bool active: stackView.currentItem
                                                 && stackView.currentItem.objectName === modelData.screen
                            property string labelText: {
                                T.lang   // 语言变更强制重估
                                return content.tabLabels[index] || modelData.screen
                            }
                            implicitWidth: compact ? 48
                                : Math.max(80, labelMetrics.width + 24 + 8 + 24)
                            implicitHeight: compact ? 48 : 44

                            TextMetrics {
                                id: labelMetrics
                                font.family: ThemeEngine.fontUi
                                font.pixelSize: 12
                                text: navBtn.labelText
                            }
                            background: Rectangle {
                                color: navBtn.active ? Qt.alpha(ThemeEngine.colors.primary, 0.12)
                                     : navBtn.hovered ? Qt.alpha(ThemeEngine.colors.primary, 0.07)
                                     : "transparent"
                                radius: ThemeEngine.radius.md
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }
                            contentItem: Item {
                                AppIcon {
                                    visible: content.compact
                                    anchors.centerIn: parent
                                    name: modelData.icon; size: 24
                                    color: navBtn.active ? ThemeEngine.colors.primary
                                                         : ThemeEngine.colors.textSecondary
                                }
                                RowLayout {
                                    visible: !content.compact
                                    anchors.centerIn: parent
                                    spacing: 8
                                    AppIcon {
                                        name: modelData.icon; size: 24
                                        color: navBtn.active ? ThemeEngine.colors.primary
                                                             : ThemeEngine.colors.textSecondary
                                    }
                                    Label {
                                        text: navBtn.labelText
                                        font.family: ThemeEngine.fontUi
                                        font.pixelSize: 12
                                        font.weight: navBtn.active ? Font.DemiBold : Font.Normal
                                        color: navBtn.active ? ThemeEngine.colors.primary
                                                             : ThemeEngine.colors.textSecondary
                                    }
                                }
                            }
                            onClicked: {
                                if (navBlocked) { closeCurrentOverlay(); return }
                                switchToTab(index)
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Item { width: compact ? 0 : 4; visible: !compact }
            }
        }
    }
}
