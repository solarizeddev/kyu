// A flat row that opens a page: title, one line under it, its key, an arrow.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: row
    property string title
    property string subtitle
    property string key
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.onPressAction: row.clicked()

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.gridUnit
        anchors.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            QQC2.Label {
                text: row.title
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: row.subtitle
                visible: text.length > 0
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        QQC2.Label {
            text: row.key
            visible: text.length > 0
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
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
        onClicked: row.clicked()
    }
}
