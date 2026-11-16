// One package: name, then versions or status, then size, in columns.
// Status always has a mark or a word, never colour alone.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

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
    readonly property bool quiet: status === "waiting" || status === "unchanged" || (tracking && status === "pending")

    implicitHeight: Math.round(Kirigami.Units.gridUnit * 1.6)
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
        QQC2.Label {
            text: row.name
            font.family: Kirigami.Theme.fixedWidthFont.family
            elide: Text.ElideRight
            Layout.fillWidth: !row.restart
            Layout.maximumWidth: row.restart ? implicitWidth : -1
            Layout.minimumWidth: Kirigami.Units.gridUnit * 4
            color: row.quiet ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
        }
        // Updating the running kernel's package means a restart.
        Rectangle {
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
        Item {
            visible: row.restart
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: row.detailText()
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: row.working ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
        }
        QQC2.Label {
            visible: !row.tracking
            text: row.sizeText
            horizontalAlignment: Text.AlignRight
            Layout.preferredWidth: Kirigami.Units.gridUnit * 3.5
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
