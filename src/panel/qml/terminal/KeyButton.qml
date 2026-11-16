// Terminal style: a key-bar button, "⏎ update all": the key in the accent
// colour, then what it does. `primary` fills it with the selection colour.
import QtQuick
import QtQuick.Controls as QQC2
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
    implicitWidth: leftPadding + rightPadding + keyLabel.implicitWidth + gap + label.implicitWidth
    implicitHeight: topPadding + bottomPadding + Math.max(keyLabel.implicitHeight, label.implicitHeight)

    contentItem: Row {
        spacing: button.gap
        QQC2.Label {
            id: keyLabel
            text: button.key
            visible: text.length > 0
            font: button.font
            color: button.primary ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.linkColor
            opacity: button.enabled ? 1 : 0.5
        }
        QQC2.Label {
            id: label
            text: button.text
            font: button.font
            elide: Text.ElideRight
            width: Math.max(0, Math.min(implicitWidth, button.availableWidth - (keyLabel.visible ? keyLabel.implicitWidth + button.gap : 0)))
            color: button.primary ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            opacity: button.enabled ? 1 : 0.5
        }
    }
    background: Rectangle {
        color: button.primary ? (button.enabled ? (button.down ? Qt.darker(Kirigami.Theme.highlightColor, 1.2) : button.hovered ? Qt.lighter(Kirigami.Theme.highlightColor, 1.1) : Kirigami.Theme.highlightColor)
                                                 : Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.4))
             : button.down || button.hovered ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.1)
             : "transparent"
        border.width: button.visualFocus ? 1 : 0
        border.color: Kirigami.Theme.textColor
    }
}
