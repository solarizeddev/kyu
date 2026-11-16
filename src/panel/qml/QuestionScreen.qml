// pacman waits for an answer. Options worded from what pacman asks, pacman's
// default marked, pacman's own text shown verbatim.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: screen
    signal showOutput()
    property string selected: Panel.questionDefault
    readonly property bool hasOptions: Panel.questionOptions.length > 0
    spacing: Kirigami.Units.largeSpacing * 2

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.gridUnit
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        QQC2.Label {
            text: "Updates"
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: Panel.pausedText
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.smallSpacing
        Kirigami.Heading {
            level: 1
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.6
            font.weight: Font.DemiBold
            text: Panel.questionTitle
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: Panel.questionText
            visible: text.length > 0
            wrapMode: Text.Wrap
            color: Kirigami.Theme.disabledTextColor
            Layout.fillWidth: true
        }
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Kirigami.Units.largeSpacing

            QQC2.ButtonGroup { id: answers }

            Repeater {
                model: Panel.questionOptions
                delegate: Rectangle {
                    id: option
                    required property var modelData
                    readonly property bool checked: screen.selected === modelData.value
                    Layout.fillWidth: true
                    implicitHeight: optionRow.implicitHeight + Kirigami.Units.largeSpacing * 2
                    Kirigami.Theme.colorSet: Kirigami.Theme.View
                    Kirigami.Theme.inherit: false
                    color: Kirigami.Theme.backgroundColor
                    radius: Kirigami.Units.largeSpacing
                    border.width: checked ? 1 : 0
                    border.color: Kirigami.Theme.highlightColor

                    RowLayout {
                        id: optionRow
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.largeSpacing
                        anchors.rightMargin: Kirigami.Units.largeSpacing * 2
                        QQC2.RadioButton {
                            QQC2.ButtonGroup.group: answers
                            checked: option.checked
                            enabled: Panel.canAnswer
                            onToggled: if (checked) screen.selected = option.modelData.value
                        }
                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true
                            QQC2.Label {
                                text: option.modelData.label
                                font.family: Kirigami.Theme.fixedWidthFont.family
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            QQC2.Label {
                                text: option.modelData.detail
                                visible: text.length > 0
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: Panel.canAnswer
                        onClicked: screen.selected = option.modelData.value
                        z: -1
                    }
                }
            }

            QQC2.Label {
                text: "pacman:\n" + Panel.questionPacman
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
            }
        }
    }

    Message {
        text: Panel.message
        iconName: "dialog-error"
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    QQC2.Label {
        text: Panel.questionNote
        visible: text.length > 0
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.smallSpacing
        QQC2.Button {
            visible: screen.hasOptions
            text: Panel.replying ? "Sending answer" : "Continue"
            highlighted: true
            enabled: Panel.canAnswer && !Panel.replying && screen.selected.length > 0
            onClicked: Panel.answer(screen.selected)
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
            font.weight: Font.DemiBold
        }
        QQC2.Button {
            text: "Cancel the update"
            flat: screen.hasOptions
            highlighted: !screen.hasOptions
            enabled: Panel.canAnswer && !Panel.replying
            onClicked: Panel.cancelUpdate()
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.25
        }
    }
}
