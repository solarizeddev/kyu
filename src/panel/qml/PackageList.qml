// Packages grouped by repo or remote; the only grouping Kyu uses.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Card {
    id: card
    property bool tracking: false // show per-package transaction status

    ListView {
        id: list
        anchors.fill: parent
        model: Panel.updates
        boundsBehavior: Flickable.StopAtBounds
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

        section.property: "group"
        section.delegate: Rectangle {
            required property string section
            width: ListView.view.width
            implicitHeight: Kirigami.Units.gridUnit * 1.5
            color: Kirigami.Theme.alternateBackgroundColor
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.largeSpacing * 2
                anchors.rightMargin: Kirigami.Units.largeSpacing * 2
                QQC2.Label {
                    text: section
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    text: Panel.updates.countInGroup(section)
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        delegate: PackageRow {
            width: ListView.view.width
            tracking: card.tracking
        }

        // Keep the package pacman is working on in view.
        Connections {
            target: Panel.updates
            enabled: card.tracking
            function onWorkingRowChanged() {
                if (Panel.updates.workingRow >= 0) {
                    list.positionViewAtIndex(Panel.updates.workingRow, ListView.Contain)
                }
            }
        }
    }
}
