// Packages pacman changed in this update, in the order it changed them.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Style.js" as Style

ColumnLayout {
    signal back()
    spacing: Kirigami.Units.largeSpacing

    PageHeader { title: "What changed"; onBack: parent.back() }

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
                              : modelData.action === "updating" ? "updated"
                              : modelData.action === "installing" ? "installed" + (modelData.newVersion.length > 0 ? " " + modelData.newVersion : "")
                              : modelData.action + " " + modelData.newVersion
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
        }
    }
}
