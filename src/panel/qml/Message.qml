// A fact the user should see: a failed check, a refused start, a question.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: message
    property alias text: label.text
    property string iconName: "dialog-warning"

    visible: text.length > 0
    color: Kirigami.Theme.neutralBackgroundColor
    radius: Kirigami.Units.largeSpacing
    implicitHeight: row.implicitHeight + Kirigami.Units.largeSpacing * 2
    Layout.fillWidth: true

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing
        Kirigami.Icon {
            source: message.iconName
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
            Layout.alignment: Qt.AlignTop
        }
        QQC2.Label {
            id: label
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Kirigami.Theme.textColor
        }
    }
}
