// One package: name, then versions or status, then size, in columns.
// Status always has a mark or a word, never colour alone.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".."
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
    // Very narrow windows drop the size column first.
    readonly property bool narrow: width < Kirigami.Units.gridUnit * 22
    readonly property bool quiet: status === "waiting" || status === "unchanged" || (tracking && status === "pending")

    implicitHeight: Math.round(Kirigami.Units.gridUnit * 1.6)

    HoverHandler { id: hover }
    QQC2.ToolTip.visible: hover.hovered && (detail.truncated || nameLabel.truncated)
    QQC2.ToolTip.text: row.name + "  " + row.detailText()
    color: working ? Kirigami.Theme.alternateBackgroundColor : "transparent"

    function detailText() {
        switch (status) {
        case "waiting": return "waiting"
        case "working": return action
        case "failed": return "failed"
        case "unchanged": return "not changed"
        case "done":
            if (action === "removing") return "removed"
            return newVersion
        }
        if (kind === "flatpak" || oldVersion.length === 0) return newVersion
        return oldVersion + " → " + newVersion
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.gridUnit
        anchors.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            visible: row.tracking
            Layout.preferredWidth: Kirigami.Units.gridUnit * 0.8
            text: row.status === "done" ? "✓" : row.status === "failed" ? "✗" : row.working ? "•" : ""
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.weight: Font.Bold
            color: row.status === "failed" ? Kirigami.Theme.negativeTextColor
                 : row.status === "done" ? Kirigami.Theme.positiveTextColor
                 : Kirigami.Theme.highlightColor
            Accessible.ignored: true
        }
        // Columns: name (+ restart chip), versions or status, size. When the
        // row is too narrow, the name and the versions both give way toward a
        // few characters each (elided; full text on hover); extra width goes
        // to the gap between them.
        // Measured in the label's own (fixed-width) font: implicitWidth can be
        // stale from the default font and elide short names needlessly.
        TextMetrics { id: nameMetrics; font: nameLabel.font; text: nameLabel.text }
        TextMetrics { id: detailMetrics; font: detail.font; text: detail.text }
        RowLayout {
            id: nameGroup
            readonly property real fullWidth: Math.ceil(nameMetrics.advanceWidth) + 1 + (row.restart ? restartChip.implicitWidth + spacing : 0)
            spacing: Kirigami.Units.largeSpacing
            Layout.fillWidth: true
            Layout.preferredWidth: fullWidth
            Layout.maximumWidth: fullWidth
            Layout.minimumWidth: nameLabel.Layout.minimumWidth + (row.restart ? restartChip.implicitWidth + spacing : 0)
            QQC2.Label {
                id: nameLabel
                text: row.name
                font.family: Kirigami.Theme.fixedWidthFont.family
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.maximumWidth: Math.ceil(nameMetrics.advanceWidth) + 1
                Layout.minimumWidth: Math.min(Math.ceil(nameMetrics.advanceWidth) + 1, Kirigami.Units.gridUnit * 4)
                color: row.quiet ? Style.muted(Kirigami.Theme.textColor) : Kirigami.Theme.textColor
            }
            // Updating the running kernel's package means a restart.
            Rectangle {
                id: restartChip
                visible: row.restart
                color: "transparent"
                border.width: 1
                border.color: Kirigami.Theme.neutralTextColor
                radius: Kirigami.Units.smallSpacing / 2
                implicitWidth: chip.implicitWidth + Kirigami.Units.largeSpacing
                implicitHeight: chip.implicitHeight + 2
                QQC2.Label {
                    id: chip
                    anchors.centerIn: parent
                    text: "restart"
                    font.family: Kirigami.Theme.fixedWidthFont.family
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.neutralTextColor
                }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
        }
        QQC2.Label {
            id: detail
            text: row.detailText()
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: row.working ? Kirigami.Theme.textColor : Style.muted(Kirigami.Theme.textColor)
            // Long versions (git snapshots) give way from the left, so the
            // new version stays readable.
            elide: Text.ElideLeft
            horizontalAlignment: Text.AlignRight
            Layout.fillWidth: true
            Layout.preferredWidth: Math.ceil(detailMetrics.advanceWidth) + 1
            Layout.maximumWidth: Math.ceil(detailMetrics.advanceWidth) + 1
            Layout.minimumWidth: Math.min(Math.ceil(detailMetrics.advanceWidth) + 1, Kirigami.Units.gridUnit * 3)
        }
        QQC2.Label {
            visible: !row.tracking && !row.narrow
            text: row.sizeText
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: Kirigami.Units.gridUnit * 3.5
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Style.muted(Kirigami.Theme.textColor)
        }
    }
}
