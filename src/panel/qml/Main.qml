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

    // The page on top of the current screen ("" when none): keys depend on it.
    property string page: ""
    function push(component, name, properties) {
        stack.push(component, properties || {})
        page = name
    }
    function back() {
        stack.pop()
        page = stack.depth > 1 ? page : ""
    }
    function showOutput() {
        push(outputPage, "output")
    }
    function showChanges() {
        push(changesPage, "changes")
    }
    function showConfigs() {
        push(configsPage, "configs")
    }
    function showDiff(path, saved) {
        push(diffPage, "diff", { path: path, saved: saved })
    }
    function showProcesses() {
        push(processesPage, "processes")
    }

    // Keyboard. Every key here is named in the screen's hint line. Nothing
    // here restarts the computer or cancels an update: those stay buttons.
    readonly property bool onScreen: stack.depth === 1
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.onScreen && Panel.screen === "list" && Panel.canUpdate
        onActivated: Panel.updateAll()
    }
    Shortcut {
        sequence: "C"
        enabled: root.onScreen && Panel.screen === "list" && !Panel.checking
        onActivated: Panel.checkAgain()
    }
    Shortcut {
        sequence: "L"
        enabled: root.onScreen && Panel.screen !== "list"
        onActivated: root.showOutput()
    }
    Shortcut {
        sequence: "S"
        enabled: root.onScreen && Panel.screen === "finished" && Panel.configs.length > 0
        onActivated: root.showConfigs()
    }
    Shortcut {
        sequence: "P"
        enabled: root.onScreen && Panel.screen === "finished" && Panel.processCount > 0
        onActivated: root.showProcesses()
    }
    Shortcut {
        sequence: "W"
        enabled: root.onScreen && Panel.screen === "finished" && Panel.changes.length > 0
        onActivated: root.showChanges()
    }
    Shortcut {
        sequence: "Y"
        enabled: root.page === "output"
        onActivated: Panel.copyLog()
    }
    Shortcut {
        // Back from a page; "done" on the result; otherwise close the window.
        // Closing never stops an update or answers a question.
        sequence: "Escape"
        onActivated: {
            if (stack.depth > 1) {
                root.back()
            } else if (Panel.screen === "finished") {
                Panel.done()
            } else {
                root.close()
            }
        }
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
            onShowConfigs: root.showConfigs()
            onShowProcesses: root.showProcesses()
        }
    }
    Component { id: outputPage; OutputPage { onBack: root.back() } }
    Component { id: changesPage; ChangesPage { onBack: root.back() } }
    Component {
        id: configsPage
        ConfigsPage {
            onBack: root.back()
            onShowDiff: (path, saved) => root.showDiff(path, saved)
        }
    }
    Component { id: diffPage; DiffPage { onBack: root.back() } }
    Component { id: processesPage; ProcessesPage { onBack: root.back() } }
}
