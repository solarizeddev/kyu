// pacman's output as the helper relayed it, with the exact command.
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
        Layout.rightMargin: Kirigami.Units.gridUnit
        QQC2.ToolButton {
            icon.name: "go-previous"
            text: "Back"
            display: QQC2.AbstractButton.IconOnly
            onClicked: back()
        }
        Kirigami.Heading {
            level: 2
            text: "Technical details"
            Layout.fillWidth: true
        }
        Kirigami.Heading {
            level: 2
            visible: Panel.screen === "running" || Panel.screen === "question"
            text: Panel.percent + "%"
        }
    }
    ThinBar {
        visible: Panel.screen === "running"
        value: Panel.percent / 100
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    QQC2.Label {
        text: Panel.processLine
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    // The output on the View colour set's background, edge to edge.
    Rectangle {
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: lines
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            anchors.leftMargin: Kirigami.Units.gridUnit
            anchors.rightMargin: Kirigami.Units.gridUnit
            clip: true
            model: Panel.output
            boundsBehavior: Flickable.StopAtBounds
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
            property bool following: true
            onMovementEnded: following = atYEnd
            onCountChanged: if (following) Qt.callLater(positionViewAtEnd)
            Component.onCompleted: positionViewAtEnd()

            delegate: QQC2.Label {
                required property string line
                required property string stream
                width: ListView.view.width
                text: line.length > 0 ? line : " "
                wrapMode: Text.WrapAnywhere
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: stream === "kyu" ? Kirigami.Theme.linkColor
                     : stream === "stderr" ? Kirigami.Theme.neutralTextColor
                     : Kirigami.Theme.textColor
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing
        QQC2.Button {
            text: "Copy log"
            icon.name: "edit-copy"
            onClicked: Panel.copyLog()
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.25
        }
        QQC2.Button {
            text: "Follow in Konsole"
            icon.name: "utilities-terminal"
            onClicked: Panel.followInTerminal()
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.25
        }
    }

    Message {
        text: Panel.message
        iconName: "dialog-error"
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    KeyHints {
        text: "y copy log · esc back"
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }
    QQC2.Label {
        text: "Exactly the commands shown run, nothing hidden." + (Panel.logPath.length > 0 ? " Full log: " + Panel.logPath : "")
        wrapMode: Text.WrapAnywhere
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit
    }
}
