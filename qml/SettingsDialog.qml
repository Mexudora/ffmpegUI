import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FFmpegUI

Dialog {
    id: settingsDialog
    required property ToolSettings tools
    property bool analysisBusy: false
    modal: true
    title: qsTr("Settings")
    width: Math.min(660, parent.width - 32)
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape

    onOpened: {
        directoryField.text = tools.directory
        tools.clearError()
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Select the folder containing ffmpeg.exe and ffprobe.exe")
        onAccepted: directoryField.text = tools.pathFromUrl(selectedFolder)
    }

    contentItem: ColumnLayout {
        spacing: 12
        Label { text: qsTr("FFmpeg directory") }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: directoryField
                Layout.fillWidth: true
                enabled: !settingsDialog.analysisBusy
                placeholderText: qsTr("For example: C:/ffmpeg/bin")
                selectByMouse: true
            }
            Button {
                text: qsTr("Choose…")
                enabled: !settingsDialog.analysisBusy
                onClicked: folderDialog.open()
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Select a directory containing both executables. FFmpeg is installed separately. Existing queue jobs keep their saved executable path.")
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            visible: settingsDialog.analysisBusy
            text: qsTr("Wait until file analysis has finished before changing the directory.")
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: settingsDialog.tools.error
            textFormat: Text.PlainText
            color: "#c62828"
            wrapMode: Text.WordWrap
        }
    }
    footer: DialogButtonBox {
        Button {
            text: qsTr("Save")
            // ActionRole keeps the dialog open when validation or saving fails.
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            enabled: !settingsDialog.analysisBusy
            onClicked: {
                if (settingsDialog.tools.saveDirectory(directoryField.text))
                    settingsDialog.close()
            }
        }
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: settingsDialog.close()
        }
    }
}
