import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FFmpegUI

ApplicationWindow {
    id: window
    width: 1500
    height: 1000
    minimumWidth: 1080
    minimumHeight: 800
    visible: true
    title: qsTr("ffmpegUI")

    MediaWorkspace { id: workspaceModel }
    ConversionQueue { id: conversionQueue }

    SettingsDialog {
        id: applicationSettings
        parent: Overlay.overlay
        tools: workspaceModel.tools
        analysisBusy: workspaceModel.busy
    }

    onClosing: function(close) {
        if (conversionQueue.running || conversionQueue.busy) {
            close.accepted = false
            conversionQueue.requestStopAfterCurrent()
            conversionQueue.cancelCurrent()
        }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open a file for inspection")
        fileMode: FileDialog.OpenFile
        onAccepted: workspaceModel.openFile(selectedFile)
    }
    FileDialog {
        id: addDialog
        title: qsTr("Add files to preparation")
        fileMode: FileDialog.OpenFiles
        onAccepted: workspaceModel.addFiles(selectedFiles)
    }
    FolderDialog {
        id: inputFolderDialog
        title: qsTr("Add files from a directory")
        onAccepted: workspaceModel.addFolder(selectedFolder)
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            Button { text: qsTr("Open"); onClicked: openDialog.open() }
            Button { text: qsTr("Add files"); onClicked: addDialog.open() }
            Button { text: qsTr("Add directory"); onClicked: inputFolderDialog.open() }
            Button { text: qsTr("Settings"); onClicked: applicationSettings.open() }
            Item { Layout.fillWidth: true }
            Label { text: workspaceModel.busy ? qsTr("Analyzing files…") : "" }
            BusyIndicator {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                visible: workspaceModel.busy
                running: workspaceModel.busy
            }
        }
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 12
        columns: 2
        columnSpacing: 12
        rowSpacing: 12
        InspectorPanel {
            Layout.row: 0
            Layout.column: 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            Layout.preferredHeight: 360
            Layout.minimumWidth: 0
            workspace: workspaceModel
        }
        QueuePanel {
            Layout.row: 0
            Layout.column: 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            Layout.preferredHeight: 360
            Layout.minimumWidth: 0
            queue: conversionQueue
        }
        MatrixPanel {
            Layout.row: 1
            Layout.column: 0
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 500
            Layout.minimumWidth: 0
            workspace: workspaceModel
            queue: conversionQueue
        }
    }
}
