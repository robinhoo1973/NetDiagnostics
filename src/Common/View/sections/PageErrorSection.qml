// PageErrorSection.qml — DetailPage 错误块（page-detail.md §2.4）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.sm
    // 5WHY (2026-09-27 Warning 塌缩): errorOutput 单字段承载三级严重度——
    // 曾恒 fail 配色，「证书 20 天到期」与「连接失败」同红，与 hero 黄色
    // Warning 状态词同屏矛盾。按 statusToken 稳定名分级（DiagId.h 描述符表
    // 单一事实源；序值比对是已知枚举重排事故类，5WHY 2026-09-27）。
    readonly property bool _isWarning: detailData.statusToken === "warning"
    cardColor: _isWarning ? Qt.alpha(ThemeEngine.colors.warning, 0.08)
                          : Qt.alpha(ThemeEngine.colors.fail, 0.06)
    borderColor: _isWarning ? Qt.alpha(ThemeEngine.colors.warning, 0.5)
                            : Qt.alpha(ThemeEngine.colors.fail, 0.5)

    property var detailData: ({})
    readonly property bool _hasError: (detailData.errorOutput || "") !== ""
    active: _hasError && detailData.showErrorOutput !== false

    Label {
        Layout.fillWidth: true
        text: detailData.errorOutput || ""
        color: _isWarning ? ThemeEngine.colors.warningStrong : ThemeEngine.colors.fail
        font.family: ThemeEngine.fontUi
        font.pixelSize: ThemeEngine.fontSize.body
        wrapMode: Text.WordWrap
    }
}
