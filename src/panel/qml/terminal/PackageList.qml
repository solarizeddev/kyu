// Terminal style: the updates in aligned columns, grouped by repo or remote
// ("core ┄┄┄ 2"). While updating (tracking), marks and words say where each
// package stands, and the list follows the one pacman is working on.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../Style.js" as Style

ListView {
    id: list
    property bool tracking: false
    clip: true
    model: Panel.updates
    boundsBehavior: Flickable.StopAtBounds
    Layout.minimumHeight: Kirigami.Units.gridUnit * 3
    readonly property real rowWidth: width - (bar.visible ? bar.width : 0)
    QQC2.ScrollBar.vertical: QQC2.ScrollBar { id: bar }
    Accessible.name: tracking ? "Packages being updated" : "Pending updates"

    // One width for the size column: the widest size in the list.
    TextMetrics {
        id: sizeMetrics
        text: "999 MiB"
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pointSize: Kirigami.Theme.defaultFont.pointSize
    }

    section.property: "group"
    section.delegate: Item {
        required property string section
        width: list.rowWidth
        implicitHeight: sectionRow.implicitHeight + Kirigami.Units.largeSpacing
        RowLayout {
            id: sectionRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Kirigami.Units.gridUnit
            anchors.rightMargin: Kirigami.Units.gridUnit
            spacing: Kirigami.Units.largeSpacing
            QQC2.Label {
                text: section
                font.weight: Font.Bold
                color: Style.muted(Kirigami.Theme.textColor)
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                implicitHeight: 1
                color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2)
            }
            QQC2.Label {
                text: Panel.updates.countInGroup(section)
                color: Style.muted(Kirigami.Theme.textColor)
            }
        }
    }

    delegate: PackageRow {
        width: list.rowWidth
        tracking: list.tracking
        sizeWidth: Math.ceil(sizeMetrics.advanceWidth)
    }

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
