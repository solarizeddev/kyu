// Kyu update panel. Layout only: state and actions live in PanelController,
// exposed as the context property "Panel". Colours, fonts, spacing and icons
// come from Kirigami.Theme / Kirigami.Units and the icon theme, never from
// literals (HANDOFF.md rule 6).
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.ApplicationWindow {
    id: root

    width: Kirigami.Units.gridUnit * 24
    height: Kirigami.Units.gridUnit * 36
    minimumWidth: Kirigami.Units.gridUnit * 18
    minimumHeight: Kirigami.Units.gridUnit * 20
    visible: true
    title: "Updates"
    color: Kirigami.Theme.backgroundColor

    function showOutput() {
        stack.push(outputPage)
    }
    function showChanges() {
        stack.push(changesPage)
    }

    Connections {
        target: Panel
        function onActivateRequested() {
            root.show()
            root.raise()
            root.requestActivate()
        }
    }

    QQC2.StackView {
        id: stack
        anchors.fill: parent
        initialItem: Item {
            Loader {
                anchors.fill: parent
                sourceComponent: Panel.screen === "running" ? runningScreen
                               : Panel.screen === "question" ? questionScreen
                               : Panel.screen === "finished" ? finishedScreen
                               : listScreen
            }
        }
    }

    Component { id: listScreen; ListScreen {} }
    Component { id: runningScreen; RunningScreen { onShowOutput: root.showOutput() } }
    Component { id: questionScreen; QuestionScreen { onShowOutput: root.showOutput() } }
    Component {
        id: finishedScreen
        FinishedScreen {
            onShowOutput: root.showOutput()
            onShowChanges: root.showChanges()
        }
    }
    Component { id: outputPage; OutputPage { onBack: stack.pop() } }
    Component { id: changesPage; ChangesPage { onBack: stack.pop() } }
}
