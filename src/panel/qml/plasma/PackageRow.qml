// Plasma style: one package on two lines. The name in the normal font (and
// "Restart" when it replaces the running kernel), the versions under it in
// the fixed-width font, the download size on the right. While updating, a
// mark and a word say where it stands (never colour alone).
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

    readonly property bool working: status === "working"
    readonly property bool quiet: tracking && (status === "waiting" || status === "pending" || status === "unchanged")

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    color: working ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.15) : "transparent"

    HoverHandler { id: hover }
    QQC2.ToolTip.visible: hover.hovered && (versions.truncated || nameLabel.truncated)
    QQC2.ToolTip.text: row.name + "\n" + row.versionText()

    function versionText() {
        if (kind === "flatpak" || oldVersion.length === 0) return newVersion
        return oldVersion + " → " + newVersion
    }
    function stateText() {
        switch (status) {
        case "waiting": return "Waiting"
        case "working": return action.length > 0 ? action.charAt(0).toUpperCase() + action.slice(1) + "…" : "Working…"
        case "failed": return "Failed"
        case "unchanged": return "Not changed"
        case "done": return action === "removing" ? "Removed" : action === "installing" ? "Installed" : "Updated"
        }
        return ""
    }

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Kirigami.Units.gridUnit
        anchors.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            visible: row.tracking
            source: row.status === "done" ? "emblem-success" : row.status === "failed" ? "emblem-error" : ""
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
            QQC2.BusyIndicator {
                anchors.fill: parent
                visible: row.working
                running: visible
            }
        }
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            RowLayout {
                id: nameRow
                spacing: Kirigami.Units.smallSpacing * 2
                Layout.fillWidth: true
                // The name takes what it needs, up to the room left beside the
                // chip; a spacer takes the rest (a layout column is otherwise
                // only as wide as its widest child allows).
                QQC2.Label {
                    id: nameLabel
                    text: row.name
                    elide: Text.ElideRight
                    color: row.quiet ? Style.muted(Kirigami.Theme.textColor) : Kirigami.Theme.textColor
                    // Measured: an eliding label's implicitWidth is not
                    // reliable, and short names got cut with room to spare.
                    readonly property real fullWidth: Math.ceil(nameMetrics.advanceWidth) + 1
                    Layout.fillWidth: true
                    Layout.preferredWidth: fullWidth
                    Layout.maximumWidth: fullWidth
                    Layout.minimumWidth: Math.min(fullWidth, Kirigami.Units.gridUnit * 3)
                    TextMetrics { id: nameMetrics; font: nameLabel.font; text: nameLabel.text }
                }
                Rectangle {
                    id: restartChip
                    visible: row.restart && !row.tracking
                    color: "transparent"
                    border.width: 1
                    border.color: Kirigami.Theme.neutralTextColor
                    radius: Kirigami.Units.smallSpacing / 2
                    implicitWidth: chip.implicitWidth + Kirigami.Units.largeSpacing
                    implicitHeight: chip.implicitHeight + 2
                    QQC2.Label {
                        id: chip
                        anchors.centerIn: parent
                        text: "Restart"
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Kirigami.Theme.neutralTextColor
                    }
                }
                Item { Layout.fillWidth: true; Layout.preferredWidth: 0 }
            }
            QQC2.Label {
                id: versions
                visible: !row.tracking
                text: row.versionText()
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Style.muted(Kirigami.Theme.textColor)
                // Long versions (git snapshots) give way from the left, so the
                // new version stays readable.
                elide: Text.ElideLeft
                Layout.fillWidth: true
            }
        }
        QQC2.Label {
            text: row.tracking ? row.stateText() : row.sizeText
            visible: text.length > 0
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: row.tracking && row.status === "done" ? Kirigami.Theme.positiveTextColor
                 : row.tracking && row.status === "failed" ? Kirigami.Theme.negativeTextColor
                 : row.working ? Kirigami.Theme.textColor
                 : Style.muted(Kirigami.Theme.textColor)
            horizontalAlignment: Text.AlignRight
        }
    }
}
