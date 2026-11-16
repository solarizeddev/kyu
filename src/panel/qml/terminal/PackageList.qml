// Packages grouped by repo or remote; the only grouping Kyu uses. Dense,
// flat rows; data in the fixed-width font.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

ListView {
    id: list
    property bool tracking: false // show per-package transaction status

    clip: true
    model: Panel.updates
    // Always room for a couple of rows, whatever else is on screen.
    Layout.minimumHeight: Math.round(Kirigami.Units.gridUnit * 3.2)
    boundsBehavior: Flickable.StopAtBounds
    QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
    Accessible.name: tracking ? "Packages being updated" : "Pending updates"

    section.property: "group"
    section.delegate: Item {
        required property string section
        width: ListView.view.width
        implicitHeight: Math.round(Kirigami.Units.gridUnit * 1.4)
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.gridUnit
            anchors.rightMargin: Kirigami.Units.gridUnit
            QQC2.Label {
                text: section
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Style.muted(Kirigami.Theme.textColor)
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: Panel.updates.countInGroup(section)
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Style.muted(Kirigami.Theme.textColor)
            }
        }
        Kirigami.Separator {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    delegate: PackageRow {
        width: ListView.view.width
        tracking: list.tracking
    }

    // Keep the package pacman is working on in view.
    Connections {
        target: Panel.updates
        enabled: list.tracking
        function onWorkingRowChanged() {
            if (Panel.updates.workingRow >= 0) {
                list.positionViewAtIndex(Panel.updates.workingRow, ListView.Contain)
            }
        }
    }
}
