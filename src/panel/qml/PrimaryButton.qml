// The one main action of a screen, filled with the accent colour: Breeze
// draws `highlighted` buttons like any other, so it is drawn here, from the
// colour scheme's own selection colours.
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.Button {
    id: button
    font.weight: Font.DemiBold
    leftPadding: Kirigami.Units.gridUnit
    rightPadding: Kirigami.Units.gridUnit
    topPadding: Kirigami.Units.largeSpacing
    bottomPadding: Kirigami.Units.largeSpacing
    // A long label ("Continue with Replace oldthing with extra/newthing")
    // ends in "…" and shows in full on hover.
    QQC2.ToolTip.visible: hovered && label.truncated
    QQC2.ToolTip.text: text

    contentItem: QQC2.Label {
        id: label
        text: button.text
        font: button.font
        color: Kirigami.Theme.highlightedTextColor
        opacity: button.enabled ? 1 : 0.6
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Kirigami.Units.smallSpacing
        color: !button.enabled ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.45)
             : button.down ? Qt.darker(Kirigami.Theme.highlightColor, 1.2)
             : button.hovered ? Qt.lighter(Kirigami.Theme.highlightColor, 1.1)
             : Kirigami.Theme.highlightColor
        // Keyboard focus: a ring in the text colour, visible on the accent.
        border.width: button.visualFocus ? 2 : 0
        border.color: Kirigami.Theme.textColor
    }
}
