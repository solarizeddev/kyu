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
    required property int index
    property bool tracking: false

    readonly property bool working: status === "working"

    implicitHeight: Kirigami.Units.gridUnit * 2.5
    color: "transparent"
    border.width: working ? 1 : 0
    border.color: Kirigami.Theme.highlightColor

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
        if (kind === "flatpak") return newVersion
        if (oldVersion.length === 0) return newVersion
        return oldVersion + " → " + newVersion
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.largeSpacing * 2
        anchors.rightMargin: Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing

        Item {
            visible: row.tracking
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
            Kirigami.Icon {
                anchors.fill: parent
                visible: row.status === "done" || row.status === "failed"
                source: row.status === "failed" ? "dialog-error" : "dialog-ok-apply"
                color: row.status === "failed" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
            }
            QQC2.BusyIndicator {
                anchors.centerIn: parent
                width: Kirigami.Units.iconSizes.smallMedium
                height: width
                padding: 0
                visible: row.working
                running: visible
            }
        }
        QQC2.Label {
            text: row.name
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.weight: Font.Medium
            elide: Text.ElideRight
            Layout.fillWidth: !row.restart
            color: row.status === "waiting" || row.status === "unchanged" || (row.tracking && row.status === "pending")
                   ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
        }
        // Updating the running kernel's package means a restart.
        Rectangle {
            visible: row.restart
            color: Kirigami.Theme.neutralBackgroundColor
            radius: Kirigami.Units.smallSpacing
            implicitWidth: chip.implicitWidth + Kirigami.Units.largeSpacing
            implicitHeight: chip.implicitHeight + Kirigami.Units.smallSpacing / 2
            QQC2.Label {
                id: chip
                anchors.centerIn: parent
                text: "restart"
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.textColor
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
    }

    Kirigami.Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !row.working
    }
}
