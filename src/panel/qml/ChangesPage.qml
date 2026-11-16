// Packages pacman changed in this update, in the order it changed them.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    signal back()
    spacing: Kirigami.Units.largeSpacing

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing * 2
        Layout.leftMargin: Kirigami.Units.largeSpacing
        QQC2.ToolButton {
            icon.name: "go-previous"
            text: "Back"
            display: QQC2.AbstractButton.IconOnly
            onClicked: back()
        }
        Kirigami.Heading {
            level: 2
            text: "What changed"
            Layout.fillWidth: true
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit
        ListView {
            anchors.fill: parent
            model: Panel.changes
            boundsBehavior: Flickable.StopAtBounds
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
            delegate: Item {
                required property var modelData
                width: ListView.view.width
                implicitHeight: Kirigami.Units.gridUnit * 2.5
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.largeSpacing * 2
                    anchors.rightMargin: Kirigami.Units.largeSpacing * 2
                    QQC2.Label {
                        text: modelData.name
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: modelData.action === "upgrading" && modelData.oldVersion.length > 0
                              ? modelData.oldVersion + " → " + modelData.newVersion
                              : modelData.action === "removing" ? "removed"
                              : modelData.action === "installing" ? "installed" + (modelData.newVersion.length > 0 ? " " + modelData.newVersion : "")
                              : modelData.action + " " + modelData.newVersion
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Kirigami.Theme.disabledTextColor
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
