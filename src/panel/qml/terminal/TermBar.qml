// Terminal style: the status line at the top: "kyu", what is going on, and a
// fact on the right.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../Style.js" as Style

Rectangle {
    id: bar
    property string text
    property color textColor: Kirigami.Theme.textColor
    property string rightText
    Kirigami.Theme.colorSet: Kirigami.Theme.Header
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    Layout.fillWidth: true
    implicitHeight: row.implicitHeight + Kirigami.Units.largeSpacing * 2

    // A rule, in case the scheme's Header and View colours are alike.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15)
    }
    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.gridUnit
        anchors.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing * 1.5
        QQC2.Label {
            text: "kyu"
            font.weight: Font.Bold
            color: Kirigami.Theme.linkColor
        }
        QQC2.Label {
            text: bar.text
            color: bar.textColor
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        QQC2.Label {
            visible: text.length > 0
            text: bar.rightText
            color: Style.muted(Kirigami.Theme.textColor)
        }
    }
}
