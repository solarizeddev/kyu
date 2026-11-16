// Running programs that still use libraries the update replaced.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    signal back()
    spacing: Kirigami.Units.largeSpacing

    PageHeader { title: "Programs using replaced libraries"; onBack: parent.back() }

    QQC2.Label {
        text: "These programs mapped a library file that the update replaced, so they still run the old version. Restarting a program, logging out, or restarting the computer makes it use the new one."
            + (Panel.processCount > Panel.processes.length ? " The first " + Panel.processes.length + " of " + Panel.processCount + " are listed." : "")
        wrapMode: Text.Wrap
        color: Kirigami.Theme.disabledTextColor
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit
        ListView {
            anchors.fill: parent
            model: Panel.processes
            boundsBehavior: Flickable.StopAtBounds
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
            delegate: Item {
                required property var modelData
                width: ListView.view.width
                implicitHeight: column.implicitHeight + Kirigami.Units.largeSpacing * 2
                ColumnLayout {
                    id: column
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Kirigami.Units.largeSpacing * 2
                    anchors.rightMargin: Kirigami.Units.largeSpacing * 2
                    spacing: 0
                    QQC2.Label {
                        text: modelData.name + "  (pid " + modelData.pid + ")"
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: modelData.libraries
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                }
                Kirigami.Separator {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                }
            }
        }
    }
}
