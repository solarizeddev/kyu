// A content surface: the View colour set's background (the sketches' "tile").
import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    radius: Kirigami.Units.largeSpacing
    clip: true
}
