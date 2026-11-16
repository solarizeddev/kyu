.pragma library

// Secondary text (versions, sizes, hints, pacman's output): the colour set's
// own text colour, faded. Kirigami.Theme.disabledTextColor is for disabled
// controls and can be much fainter than this in real colour schemes.
// The focused item if Enter should click it: a push button or a row (not a
// radio or check box, where Enter means "continue"). Duck-typed, because
// `instanceof QQC2.AbstractButton` is false for Buttons on Qt 6.4.
function pressable(item) {
    return item && item.clicked !== undefined && item.checkable === false ? item : null
}

function muted(textColor) {
    return Qt.rgba(textColor.r, textColor.g, textColor.b, 0.7)
}
