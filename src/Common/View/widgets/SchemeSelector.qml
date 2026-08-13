// =============================================================================
// SchemeSelector.qml — 分组协议选择器（归档恢复：分组 + 组图标 + 组名）
//
// UI 评审 7-2：重构前版本的 target 协议下拉按 7 组分组（Web/File/Email/DB/
// Remote/Directory/Messaging），每组带图标与文字描述；恢复该形态并统一
// 单一数据源（AppState.supportedSchemes + targetScheme 同步）。
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets

ComboBox {
    id: root

    property ListModel schemeModel: ListModel {}
    property bool _sync: false

    textRole: "scheme"
    model: schemeModel
    displayText: currentText ? currentText + "://" : ""

    font.family: ThemeEngine.monoFont
    font.pixelSize: 12

    // ── 分组目录（图标 + 15 语言标签）──
    function groupIcon(group) {
        return ["internet-globe", "file-transfer", "mail", "database",
                "remote-host", "user", "protocol-stack"][group] || "circle"
    }
    function groupLabel(group) {
        return [T.tr("schemeGroupWeb"), T.tr("schemeGroupFile"), T.tr("schemeGroupEmail"),
                T.tr("schemeGroupDb"), T.tr("schemeGroupRemote"), T.tr("schemeGroupDir"),
                T.tr("schemeGroupMsg")][group] || ""
    }

    function populate() {
        _sync = true
        schemeModel.clear()
        var supported = AppState.supportedSchemes()
        var groups = [
            ["https", "http"],
            ["ftp", "ftps", "ssh", "sftp"],
            ["smtp", "smtps", "imap", "imaps", "pop3", "pop3s"],
            ["mysql", "postgresql", "redis", "mongodb"],
            ["telnet"],
            ["ldap"],
            ["mqtt"]
        ]
        for (var g = 0; g < groups.length; ++g)
            for (var i = 0; i < groups[g].length; ++i)
                if (supported.indexOf(groups[g][i]) >= 0)
                    schemeModel.append({ scheme: groups[g][i], schemeGroup: g })
        syncFromTarget()
        _sync = false
    }

    function syncFromTarget() {
        var t = AppState.targetScheme()
        for (var i = 0; i < schemeModel.count; ++i)
            if (schemeModel.get(i).scheme === t) {
                if (currentIndex !== i) currentIndex = i
                return
            }
    }

    Component.onCompleted: populate()
    Connections {
        target: AppState
        function onTargetChanged() { if (!root._sync) root.syncFromTarget() }
    }
    onCurrentTextChanged: {
        if (!_sync && currentText)
            AppState.setTarget(AppState.targetHost + AppState.targetPath, currentText)
    }

    // ── 外观：输入底 + chevron 指示（与工具栏输入框同体系）──
    background: Rectangle {
        radius: 6
        color: ThemeEngine.colors.input
        border {
            width: root.activeFocus ? 2 : 1
            color: root.activeFocus ? ThemeEngine.colors.borderFocused : ThemeEngine.colors.borderCard
        }
    }
    contentItem: Label {
        text: root.displayText
        font: root.font
        color: ThemeEngine.colors.textPrimary
        verticalAlignment: Text.AlignVCenter
        leftPadding: 8
        rightPadding: 24
        elide: Text.ElideRight
    }
    indicator: AppIcon {
        anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
        name: "chevron-down"; size: 12
        color: ThemeEngine.colors.textSecondary
    }

    // ── 分组弹窗 ──
    popup: Popup {
        y: root.height + 4
        width: Math.max(root.width, 230)
        height: Math.min(implicitHeight, 380)
        padding: 6
        background: Rectangle {
            color: ThemeEngine.colors.card
            border { width: 1; color: ThemeEngine.colors.borderCard }
            radius: 10
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }

    delegate: ItemDelegate {
        width: ListView.view ? ListView.view.width : 230
        hoverEnabled: true
        padding: 0
        leftPadding: 0
        rightPadding: 0
        height: isFirst ? 62 : 34

        readonly property int grp: model.schemeGroup
        readonly property bool isFirst: {
            var prev = model.index > 0 ? root.schemeModel.get(model.index - 1) : null
            return !prev || prev.schemeGroup !== model.schemeGroup
        }

        highlighted: model.scheme === root.currentText
        background: Rectangle {
            color: highlighted ? Qt.alpha(ThemeEngine.colors.primary, 0.12)
                 : hovered ? Qt.alpha(ThemeEngine.colors.primary, 0.05)
                 : "transparent"
            radius: 6
        }
        contentItem: ColumnLayout {
            spacing: 0
            // 组头：图标 + 组名（文字描述）
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: isFirst ? 26 : 0
                visible: isFirst
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6
                    AppIcon {
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        name: root.groupIcon(grp)
                        size: 14
                        color: ThemeEngine.colors.cyan
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.groupLabel(grp)
                        font.family: ThemeEngine.fontUi
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: ThemeEngine.colors.textMuted
                        elide: Text.ElideRight
                    }
                }
            }
            // 协议项
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignVCenter
                text: model.scheme + "://"
                font.family: ThemeEngine.monoFont
                font.pixelSize: 12
                leftPadding: 28
                color: highlighted ? ThemeEngine.colors.cyan : ThemeEngine.colors.textPrimary
            }
        }
    }
}
