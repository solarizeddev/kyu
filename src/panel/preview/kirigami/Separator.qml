// Preview stand-in for Kirigami.Separator (see preview.h).
import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    implicitHeight: 1
    implicitWidth: 1
    color: Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15))
}
