// Plasma style: the updates on a View surface, grouped by repo or remote,
// two lines per package. While updating (tracking), each row says where it
// stands and the list follows the package pacman is working on.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../Style.js" as Style

Rectangle {
    id: surface
    property bool tracking: false
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    Layout.minimumHeight: Kirigami.Units.gridUnit * 4

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: Panel.updates
        boundsBehavior: Flickable.StopAtBounds
        // Rows stop short of a scroll bar that takes room (Breeze's does).
        readonly property real rowWidth: width - (bar.visible ? bar.width : 0)
        QQC2.ScrollBar.vertical: QQC2.ScrollBar { id: bar }
        Accessible.name: surface.tracking ? "Packages being updated" : "Pending updates"

        section.property: "group"
        section.delegate: Item {
            required property string section
            width: list.rowWidth
            implicitHeight: sectionRow.implicitHeight + Kirigami.Units.largeSpacing * 2
            RowLayout {
                id: sectionRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Kirigami.Units.smallSpacing
                anchors.leftMargin: Kirigami.Units.gridUnit
                anchors.rightMargin: Kirigami.Units.gridUnit
                QQC2.Label {
                    text: section
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.weight: Font.DemiBold
                    color: Style.muted(Kirigami.Theme.textColor)
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    text: Panel.updates.countInGroup(section)
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Style.muted(Kirigami.Theme.textColor)
                }
            }
        }

        delegate: PackageRow {
            width: list.rowWidth
            tracking: surface.tracking
        }

        Connections {
            target: Panel.updates
            enabled: surface.tracking
            function onWorkingRowChanged() {
                if (Panel.updates.workingRow >= 0) {
                    list.positionViewAtIndex(Panel.updates.workingRow, ListView.Contain)
                }
            }
        }
    }
}
