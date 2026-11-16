// diff -u between a settings file and its .pacnew/.pacsave.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Style.js" as Style

ColumnLayout {
    id: page
    property string path
    property string saved
    signal back()
    spacing: Kirigami.Units.largeSpacing

    PageHeader { title: page.path; onBack: page.back() }

    QQC2.Label {
        text: "$ diff -u " + page.path + " " + page.saved
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        color: Style.muted(Kirigami.Theme.textColor)
        wrapMode: Text.WrapAnywhere
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    Rectangle {
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        radius: Kirigami.Units.largeSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit

        ListView {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            clip: true
            model: Panel.diff(page.path, page.saved)
            boundsBehavior: Flickable.StopAtBounds
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
            delegate: QQC2.Label {
                required property var modelData
                width: ListView.view.width
                text: modelData.text.length > 0 ? modelData.text : " "
                wrapMode: modelData.kind === "error" ? Text.Wrap : Text.WrapAnywhere
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: modelData.kind === "add" ? Kirigami.Theme.positiveTextColor
                     : modelData.kind === "remove" ? Kirigami.Theme.negativeTextColor
                     : modelData.kind === "hunk" || modelData.kind === "header" ? Style.muted(Kirigami.Theme.textColor)
                     : Kirigami.Theme.textColor
            }
        }
    }
}
