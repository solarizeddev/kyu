// Terminal style: a key-bar button, "⏎ update all": the key in the accent
// colour, then what it does. `primary` fills it with the selection colour.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../Style.js" as Style

QQC2.AbstractButton {
    id: button
    property string key
    property bool primary: false
    // A key that works on the screen itself ("1–3 pick"): shown, not a button.
    property bool passive: false
    hoverEnabled: !passive
    focusPolicy: passive ? Qt.NoFocus : Qt.StrongFocus
    Accessible.ignored: passive
    // In the key bar (a row layout): natural width, shortening when narrow.
    Layout.fillWidth: true
    Layout.preferredWidth: implicitWidth
    Layout.maximumWidth: implicitWidth
    // The main key keeps up to 60% of the bar; the others shorten first.
    Layout.minimumWidth: primary && parent ? Math.min(implicitWidth, parent.width * 0.6)
                       : leftPadding + rightPadding + (key.length > 0 ? keyWidth : Kirigami.Units.gridUnit * 2)
    padding: Kirigami.Units.largeSpacing
    leftPadding: Kirigami.Units.largeSpacing * 1.5
    rightPadding: Kirigami.Units.largeSpacing * 1.5
    font.weight: primary ? Font.Bold : Font.Normal
    Accessible.name: text
    QQC2.ToolTip.visible: hovered && label.truncated
    QQC2.ToolTip.text: text

    readonly property real gap: key.length > 0 ? Kirigami.Units.smallSpacing * 2 : 0
    // From the labels' own widths (not the button's), so a narrow key bar
    // can shorten the text without a binding loop.
    // Measured: an eliding label's implicitWidth is not reliable (Qt 6.4).
    TextMetrics { id: keyMetrics; font: button.font; text: button.key }
    TextMetrics { id: textMetrics; font: button.font; text: button.text }
    readonly property real keyWidth: key.length > 0 ? Math.ceil(keyMetrics.advanceWidth) + 1 : 0
    readonly property real textWidth: Math.ceil(textMetrics.advanceWidth) + 1
    implicitWidth: leftPadding + rightPadding + keyWidth + gap + textWidth
    implicitHeight: topPadding + bottomPadding + Math.max(keyLabel.implicitHeight, label.implicitHeight)

    contentItem: Row {
        spacing: button.gap
        QQC2.Label {
            id: keyLabel
            text: button.key
            visible: text.length > 0
            font: button.font
            color: button.primary && button.enabled ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.linkColor
            opacity: button.enabled ? 1 : 0.5
        }
        QQC2.Label {
            id: label
            text: button.text
            font: button.font
            elide: Text.ElideRight
            width: Math.max(0, Math.min(button.textWidth, button.availableWidth - (keyLabel.visible ? button.keyWidth + button.gap : 0)))
            color: button.primary && button.enabled ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            opacity: button.enabled ? 1 : 0.5
        }
    }
    background: Rectangle {
        // Disabled: no fill (a pale accent still reads as "press me").
        color: button.passive ? "transparent"
             : button.primary && button.enabled ? (button.down ? Qt.darker(Kirigami.Theme.highlightColor, 1.2) : button.hovered ? Qt.lighter(Kirigami.Theme.highlightColor, 1.1) : Kirigami.Theme.highlightColor)
             : button.enabled && (button.down || button.hovered) ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.1)
             : "transparent"
        border.width: button.visualFocus ? 1 : 0
        border.color: Kirigami.Theme.textColor
    }
}
