// Terminal style: a screen in the fixed-width font on the colour scheme's
// View background. Everything inside inherits the font; colours stay the
// scheme's own (accents from the link colour, fills from the selection).
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.Control {
    id: term
    default property alias content: column.data
    padding: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    font.family: Kirigami.Theme.fixedWidthFont.family
    font.pointSize: Kirigami.Theme.defaultFont.pointSize
    background: Rectangle { color: Kirigami.Theme.backgroundColor }
    contentItem: ColumnLayout {
        id: column
        spacing: 0
    }
}
