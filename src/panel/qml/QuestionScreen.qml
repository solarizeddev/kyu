// pacman waits for an answer. Options worded from what pacman asks, pacman's
// default marked, pacman's own text shown verbatim.
import QtQuick
import QtQml
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import "Style.js" as Style

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

    // Keys: the option numbers pick, Enter continues. Esc (in Main) only
    // closes the window: pacman keeps waiting and the question stays. Only
    // while the question is on screen (not under the output page). Enter on
    // a focused button (Tab to "Cancel the update") clicks that button.
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

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.smallSpacing
        QQC2.Label {
            text: Panel.pausedText.toLowerCase()
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.neutralTextColor
        }
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
            color: Style.muted(Kirigami.Theme.textColor)
            Layout.fillWidth: true
        }
    }

    Hairline {}
    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 0

            QQC2.ButtonGroup { id: answers }

            Repeater {
                model: Panel.questionOptions
                delegate: Rectangle {
                    id: option
                    required property var modelData
                    required property int index
                    readonly property bool checked: screen.selected === modelData.value
                    Layout.fillWidth: true
                    implicitHeight: Math.max(Kirigami.Units.gridUnit * 2.4, optionRow.implicitHeight + Kirigami.Units.largeSpacing)
                    color: checked ? Kirigami.Theme.alternateBackgroundColor : "transparent"

                    RowLayout {
                        id: optionRow
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.gridUnit - Kirigami.Units.smallSpacing
                        anchors.rightMargin: Kirigami.Units.gridUnit
                        spacing: Kirigami.Units.largeSpacing
                        QQC2.RadioButton {
                            QQC2.ButtonGroup.group: answers
                            checked: option.checked
                            enabled: Panel.canAnswer
                            onToggled: if (checked) screen.selected = option.modelData.value
                            Accessible.name: option.modelData.label + (option.modelData.detail.length > 0 ? ", " + option.modelData.detail : "")
                        }
                        QQC2.Label {
                            text: option.index < 9 ? String(option.index + 1) : ""
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            color: Style.muted(Kirigami.Theme.textColor)
                            Accessible.ignored: true
                        }
                        // The name is what the person picks: it keeps one line
                        // (up to 60% of the row) and the detail wraps first.
                        TextMetrics { id: labelMetrics; font: optionLabel.font; text: optionLabel.text }
                        QQC2.Label {
                            id: optionLabel
                            text: option.modelData.label
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            Layout.minimumWidth: Math.min(Math.ceil(labelMetrics.advanceWidth) + 1, optionRow.width * 0.6)
                        }
                        TextMetrics { id: detailMetrics; font: optionDetail.font; text: optionDetail.text }
                        QQC2.Label {
                            id: optionDetail
                            text: option.modelData.detail
                            visible: text.length > 0
                            font: Kirigami.Theme.smallFont
                            color: Style.muted(Kirigami.Theme.textColor)
                            horizontalAlignment: Text.AlignRight
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            Layout.preferredWidth: Math.min(Math.ceil(detailMetrics.advanceWidth) + 1, Kirigami.Units.gridUnit * 10)
                            Layout.maximumWidth: Layout.preferredWidth
                            Layout.minimumWidth: Math.min(Layout.preferredWidth, Kirigami.Units.gridUnit * 3)
                        }
                    }
                    Kirigami.Separator {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
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
                text: "pacman asked:"
                color: Style.muted(Kirigami.Theme.textColor)
                font: Kirigami.Theme.smallFont
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                Layout.topMargin: Kirigami.Units.largeSpacing * 1.5
            }
            QQC2.Label {
                text: Panel.questionPacman
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Style.muted(Kirigami.Theme.textColor)
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                Layout.bottomMargin: Kirigami.Units.largeSpacing
            }
        }
    }
    Hairline {}

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing * 1.5
        spacing: Kirigami.Units.largeSpacing
        Message {
            text: Panel.message
            iconName: "dialog-error"
        }
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
                visible: screen.hasOptions
                text: Panel.replying ? "Sending answer" : "Continue with " + screen.selectedLabel
                highlighted: true
                enabled: Panel.canAnswer && !Panel.replying && screen.selected.length > 0
                onClicked: screen.continueWithSelection()
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.2
                font.weight: Font.DemiBold
            }
            QQC2.Button {
                text: "Cancel the update"
                highlighted: !screen.hasOptions
                enabled: Panel.canAnswer && !Panel.replying
                onClicked: Panel.cancelUpdate()
                Layout.fillWidth: !screen.hasOptions
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.2
            }
        }
        KeyHints {
            text: (screen.hasOptions ? "1–" + Math.min(Panel.questionOptions.length, 9) + " pick · ⏎ continue · " : "")
                  + "esc close (pacman keeps waiting)"
        }
    }
}
