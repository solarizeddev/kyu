// Plasma style: pacman waits for an answer. The question in the header,
// options worded from what pacman asks with its default marked, pacman's own
// text verbatim under them, and the answer as the main button.
import QtQuick
import QtQml
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

ColumnLayout {
    id: screen
    signal showOutput()
    property string selected: Panel.questionDefault
    readonly property bool hasOptions: Panel.questionOptions.length > 0
    readonly property string selectedLabel: {
        for (const o of Panel.questionOptions) {
            if (o.value === selected) return o.label
        }
        return ""
    }
    spacing: 0

    function continueWithSelection() {
        if (hasOptions && Panel.canAnswer && !Panel.replying && selected.length > 0) {
            Panel.answer(selected)
        }
    }

    // Keys: the option numbers pick, Enter continues (Enter on a focused push
    // button clicks it). Only while the question is on screen.
    readonly property bool keysActive: visible && Panel.canAnswer
    Instantiator {
        model: Math.min(Panel.questionOptions.length, 9)
        delegate: Shortcut {
            required property int index
            sequence: String(index + 1)
            enabled: screen.keysActive
            onActivated: screen.selected = Panel.questionOptions[index].value
        }
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: screen.keysActive
        onActivated: {
            const button = Style.pressable(screen.Window.activeFocusItem)
            if (button) {
                button.clicked()
            } else {
                screen.continueWithSelection()
            }
        }
    }

    ScreenHeader {
        iconName: "dialog-question"
        title: Panel.questionTitle
        subtitle: Panel.pausedText + (Panel.questionText.length > 0 ? "\n" + Panel.questionText : "")
    }

    Rectangle {
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        Layout.fillWidth: true
        Layout.fillHeight: true

        QQC2.ScrollView {
            id: scroll
            anchors.fill: parent
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 0

                QQC2.ButtonGroup { id: answers }
                Repeater {
                    model: Panel.questionOptions
                    delegate: QQC2.ItemDelegate {
                        id: option
                        required property var modelData
                        required property int index
                        readonly property bool picked: screen.selected === modelData.value
                        Layout.fillWidth: true
                        leftPadding: Kirigami.Units.gridUnit
                        rightPadding: Kirigami.Units.gridUnit
                        topPadding: Kirigami.Units.largeSpacing * 1.5
                        bottomPadding: Kirigami.Units.largeSpacing * 1.5
                        enabled: Panel.canAnswer
                        onClicked: screen.selected = modelData.value
                        // The chosen answer: a tint, so its text keeps the
                        // normal colours.
                        background: Rectangle {
                            color: option.picked ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.15)
                                 : option.hovered ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.07)
                                 : "transparent"
                            Kirigami.Separator {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                            }
                        }
                        Accessible.name: modelData.label + (modelData.detail.length > 0 ? ", " + modelData.detail : "")
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing
                            QQC2.RadioButton {
                                QQC2.ButtonGroup.group: answers
                                checked: option.picked
                                onToggled: if (checked) screen.selected = option.modelData.value
                                Accessible.ignored: true
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                QQC2.Label {
                                    text: option.modelData.label
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                QQC2.Label {
                                    visible: text.length > 0
                                    text: option.modelData.detail
                                    font: Kirigami.Theme.smallFont
                                    color: Style.muted(Kirigami.Theme.textColor)
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                            QQC2.Label {
                                visible: option.index < 9
                                text: String(option.index + 1)
                                font.family: Kirigami.Theme.fixedWidthFont.family
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                color: Style.muted(Kirigami.Theme.textColor)
                                Accessible.ignored: true
                            }
                        }
                    }
                }

                QQC2.Label {
                    text: "pacman asked:"
                    font: Kirigami.Theme.smallFont
                    color: Style.muted(Kirigami.Theme.textColor)
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.gridUnit
                    Layout.rightMargin: Kirigami.Units.gridUnit
                    Layout.topMargin: Kirigami.Units.gridUnit
                }
                QQC2.Label {
                    text: Panel.questionPacman
                    font.family: Kirigami.Theme.fixedWidthFont.family
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Style.muted(Kirigami.Theme.textColor)
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.gridUnit
                    Layout.rightMargin: Kirigami.Units.gridUnit
                    Layout.bottomMargin: Kirigami.Units.gridUnit
                }
            }
        }
    }
    Kirigami.Separator { Layout.fillWidth: true }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing * 2
        Layout.leftMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing
        Notice { text: Panel.message; type: "error" }
        QQC2.Label {
            text: Panel.questionNote
            visible: text.length > 0
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            color: Style.muted(Kirigami.Theme.textColor)
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing
            QQC2.Button {
                text: "Cancel the update"
                enabled: Panel.canAnswer && !Panel.replying
                onClicked: Panel.cancelUpdate()
                Layout.fillWidth: !screen.hasOptions
            }
            Item { Layout.fillWidth: true; visible: screen.hasOptions }
            PrimaryButton {
                visible: screen.hasOptions
                text: Panel.replying ? "Sending answer…" : "Continue with " + screen.selectedLabel
                enabled: Panel.canAnswer && !Panel.replying && screen.selected.length > 0
                onClicked: screen.continueWithSelection()
                Layout.maximumWidth: screen.width * 0.6
            }
        }
    }
}
