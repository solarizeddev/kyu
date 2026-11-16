// Terminal style: pacman waits for an answer. Numbered options (pacman's
// default marked), pacman's own text verbatim, and the answer on Enter.
import QtQuick
import QtQml
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

TermScreen {
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

    TermBar {
        text: Panel.pausedText.toLowerCase()
        textColor: Kirigami.Theme.neutralTextColor
    }

    QQC2.ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.gridUnit
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing
                QQC2.Label {
                    text: "? " + Panel.questionTitle
                    font.weight: Font.Bold
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    visible: text.length > 0
                    text: Panel.questionText
                    color: Style.muted(Kirigami.Theme.textColor)
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Repeater {
                    model: Panel.questionOptions
                    delegate: QQC2.AbstractButton {
                        id: option
                        required property var modelData
                        required property int index
                        readonly property bool picked: screen.selected === modelData.value
                        Layout.fillWidth: true
                        leftPadding: Kirigami.Units.gridUnit
                        rightPadding: Kirigami.Units.gridUnit
                        topPadding: Kirigami.Units.smallSpacing * 1.5
                        bottomPadding: Kirigami.Units.smallSpacing * 1.5
                        enabled: Panel.canAnswer
                        onClicked: screen.selected = modelData.value
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: picked
                        Accessible.name: modelData.label + (modelData.detail.length > 0 ? ", " + modelData.detail : "")
                        background: Rectangle {
                            color: option.picked ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.18)
                                 : option.hovered ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
                                 : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing * 1.5
                            QQC2.Label {
                                text: (option.picked ? "(•) " : "( ) ") + (option.index < 9 ? String(option.index + 1) : " ")
                                color: option.picked ? Kirigami.Theme.linkColor : Style.muted(Kirigami.Theme.textColor)
                                font.weight: option.picked ? Font.Bold : Font.Normal
                                Layout.alignment: Qt.AlignTop
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                QQC2.Label {
                                    text: option.modelData.label
                                    font.weight: option.picked ? Font.Bold : Font.Normal
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                QQC2.Label {
                                    visible: text.length > 0
                                    text: option.modelData.detail
                                    color: Style.muted(Kirigami.Theme.textColor)
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                Layout.bottomMargin: Kirigami.Units.gridUnit
                spacing: 0
                QQC2.Label {
                    text: "pacman asked:"
                    color: Style.muted(Kirigami.Theme.textColor)
                }
                QQC2.Label {
                    text: Panel.questionPacman
                    color: Style.muted(Kirigami.Theme.textColor)
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Box {
                    title: "error"
                    text: Panel.message
                    type: "error"
                    Layout.topMargin: Kirigami.Units.largeSpacing
                }
                QQC2.Label {
                    visible: text.length > 0
                    text: Panel.questionNote
                    color: Style.muted(Kirigami.Theme.textColor)
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.largeSpacing
                }
            }
        }
    }

    KeyBar {
        KeyButton {
            visible: screen.hasOptions
            key: "⏎"
            text: Panel.replying ? "sending answer…" : "continue with " + screen.selectedLabel
            primary: true
            enabled: Panel.canAnswer && !Panel.replying && screen.selected.length > 0
            onClicked: screen.continueWithSelection()
        }
        KeyButton {
            visible: screen.hasOptions && Panel.questionOptions.length > 1
            key: "1–" + Math.min(Panel.questionOptions.length, 9)
            text: "pick"
            passive: true
        }
        KeyButton {
            text: "cancel the update"
            enabled: Panel.canAnswer && !Panel.replying
            onClicked: Panel.cancelUpdate()
        }
        KeyButton {
            key: "esc"
            text: "hide"
            onClicked: screen.Window.window.close()
        }
    }
}
