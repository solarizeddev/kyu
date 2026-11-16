// .pacnew/.pacsave files from this update.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    signal back()
    signal showDiff(string path, string saved)
    spacing: Kirigami.Units.largeSpacing

    PageHeader { title: "Settings files"; onBack: parent.back() }

    QQC2.Label {
        text: "pacman did not overwrite these files. A .pacnew file holds the package's new defaults next to your version; a .pacsave file keeps the settings of a removed package."
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
        ListView {
            anchors.fill: parent
            model: Panel.configs
            boundsBehavior: Flickable.StopAtBounds
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
            delegate: Item {
                required property var modelData
                width: ListView.view.width
                implicitHeight: Kirigami.Units.gridUnit * 3
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.largeSpacing * 2
                    anchors.rightMargin: Kirigami.Units.largeSpacing * 2
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: modelData.path
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        QQC2.Label {
                            text: modelData.kind === "pacnew" ? "new defaults in " + modelData.saved : "removed package's settings in " + modelData.saved
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                    Kirigami.Icon {
                        source: "go-next"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    }
                }
                Kirigami.Separator {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: showDiff(modelData.path, modelData.saved)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit
        QQC2.Label {
            text: "To merge them: sudo pacdiff"
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            Layout.fillWidth: true
        }
        QQC2.Button {
            text: "Copy command"
            icon.name: "edit-copy"
            onClicked: Panel.copyText("sudo pacdiff")
        }
    }
}
