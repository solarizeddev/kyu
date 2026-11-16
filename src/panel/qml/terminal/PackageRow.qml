// Terminal style: one package per line: [mark] name [↻] … old → new  size.
// The mark and the word carry the state, never colour alone.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../Style.js" as Style

Rectangle {
    id: row
    required property string name
    required property string kind
    required property string oldVersion
    required property string newVersion
    required property string status
    required property string action
    required property bool restart
    required property string sizeText
    required property int index
    property bool tracking: false
    property real sizeWidth: 0

    readonly property bool working: status === "working"
    readonly property bool quiet: tracking && (status === "waiting" || status === "pending" || status === "unchanged")
    implicitHeight: content.implicitHeight + Kirigami.Units.smallSpacing * 2
    color: working ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.18) : "transparent"

    HoverHandler { id: hover }
    QQC2.ToolTip.visible: hover.hovered && (detail.truncated || nameLabel.truncated)
    QQC2.ToolTip.text: row.name + "  " + detail.text

    function detailText() {
        switch (status) {
        case "waiting": return "waiting"
        case "working": return action
        case "failed": return "failed"
        case "unchanged": return "not changed"
        case "done": return action === "removing" ? "removed" : newVersion
        }
        if (kind === "flatpak" || oldVersion.length === 0) return newVersion
        return oldVersion + " → " + newVersion
    }

    TextMetrics { id: nameMetrics; font: nameLabel.font; text: nameLabel.text }
    TextMetrics { id: detailMetrics; font: detail.font; text: detail.text }

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Kirigami.Units.gridUnit
        anchors.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing * 1.5

        QQC2.Label {
            visible: row.tracking
            text: row.status === "done" ? "✓" : row.status === "failed" ? "✗" : row.working ? "▸" : "·"
            font.weight: Font.Bold
            color: row.status === "failed" ? Kirigami.Theme.negativeTextColor
                 : row.status === "done" ? Kirigami.Theme.positiveTextColor
                 : row.working ? Kirigami.Theme.linkColor
                 : Style.muted(Kirigami.Theme.textColor)
            Accessible.ignored: true
        }
        QQC2.Label {
            id: nameLabel
            text: row.name + (row.restart && !row.tracking ? " ↻" : "")
            elide: Text.ElideRight
            color: row.quiet ? Style.muted(Kirigami.Theme.textColor) : Kirigami.Theme.textColor
            readonly property real fullWidth: Math.ceil(nameMetrics.advanceWidth) + 1
            Layout.fillWidth: true
            Layout.preferredWidth: fullWidth
            Layout.maximumWidth: fullWidth
            Layout.minimumWidth: Math.min(fullWidth, Kirigami.Units.gridUnit * 4)
            QQC2.ToolTip.visible: row.restart && restartHover.hovered
            QQC2.ToolTip.text: "Replaces the running kernel: restart afterwards"
            HoverHandler { id: restartHover }
        }
        Item { Layout.fillWidth: true; Layout.preferredWidth: 0 }
        QQC2.Label {
            id: detail
            text: row.detailText()
            color: row.working ? Kirigami.Theme.linkColor : Style.muted(Kirigami.Theme.textColor)
            // Long versions give way from the left: the new one stays.
            elide: Text.ElideLeft
            horizontalAlignment: Text.AlignRight
            readonly property real fullWidth: Math.ceil(detailMetrics.advanceWidth) + 1
            Layout.fillWidth: true
            Layout.preferredWidth: fullWidth
            Layout.maximumWidth: fullWidth
            Layout.minimumWidth: Math.min(fullWidth, Kirigami.Units.gridUnit * 3)
        }
        QQC2.Label {
            visible: !row.tracking && row.width > Kirigami.Units.gridUnit * 20
            text: row.sizeText
            horizontalAlignment: Text.AlignRight
            color: Style.muted(Kirigami.Theme.textColor)
            Layout.preferredWidth: row.sizeWidth
        }
    }
}
