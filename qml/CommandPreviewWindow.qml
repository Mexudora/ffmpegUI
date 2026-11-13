import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: preview
    property string commandText: ""
    width: 900
    height: 420
    minimumWidth: 600
    minimumHeight: 260
    visible: false
    title: qsTr("FFmpeg command — PowerShell")
    modality: Qt.WindowModal
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        Label {
            Layout.fillWidth: true
            text: qsTr("An unset destination is shown as <output file>. This is a preview, not a command to execute yet.")
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: command
                text: preview.commandText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                textFormat: TextEdit.PlainText
            }
        }
        RowLayout {
            Button {
                text: qsTr("Copy")
                onClicked: { command.selectAll(); command.copy() }
            }
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Close"); onClicked: preview.close() }
        }
    }
}
