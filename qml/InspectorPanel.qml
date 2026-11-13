import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFmpegUI

Frame {
    id: panel
    required property MediaWorkspace workspace
    readonly property var file: workspace.selectedFile
    readonly property var info: file.metadata || ({})
    padding: 10

    function display(value) {
        return value === undefined || value === null || value === "" ? qsTr("Unknown") : String(value)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("1. File information"); font.bold: true; Layout.fillWidth: true }
            BusyIndicator {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                running: panel.workspace.busy
                visible: running
            }
        }
        ComboBox {
            Layout.fillWidth: true
            model: panel.workspace.files
            textRole: "name"
            valueRole: "id"
            currentIndex: panel.workspace.selectedIndex
            displayText: currentIndex < 0 ? qsTr("Preview / choose a prepared file") : currentText
            onActivated: panel.workspace.selectIndex(currentIndex)
        }
        ScrollView {
            id: details
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: details.availableWidth
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: panel.file.path || qsTr("Open a file from the top toolbar")
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                }
                Label {
                    visible: panel.file.loading === true
                    text: qsTr("Analyzing file…")
                }
                Label {
                    Layout.fillWidth: true
                    visible: (panel.file.error || "").length > 0
                    text: panel.file.error || ""
                    textFormat: Text.PlainText
                    color: "#b3261e"
                    wrapMode: Text.WrapAnywhere
                }
                Label {
                    visible: panel.file.ready === true
                    text: qsTr("Container: %1; duration: %2 s")
                        .arg(panel.display(panel.info.format ? panel.info.format.format_name : undefined))
                        .arg(panel.display(panel.info.format ? panel.info.format.duration : undefined))
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: panel.info.streams || []
                    delegate: Frame {
                        id: track
                        required property var modelData
                        Layout.fillWidth: true
                        contentItem: ColumnLayout {
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Stream 0:%1 — %2 — %3")
                                    .arg(panel.display(track.modelData.index))
                                    .arg(panel.display(track.modelData.codec_type))
                                    .arg(panel.display(track.modelData.codec_name))
                                textFormat: Text.PlainText
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Title: %1; language: %2")
                                    .arg(panel.display(track.modelData.tags ? track.modelData.tags.title : undefined))
                                    .arg(panel.display(track.modelData.tags ? track.modelData.tags.language : undefined))
                                textFormat: Text.PlainText
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: track.modelData.codec_type === "video"
                                text: qsTr("Resolution: %1 × %2; frame rate: %3")
                                    .arg(panel.display(track.modelData.width))
                                    .arg(panel.display(track.modelData.height))
                                    .arg(panel.display(track.modelData.avg_frame_rate))
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: track.modelData.codec_type === "audio"
                                text: qsTr("Sample rate: %1 Hz; channels: %2")
                                    .arg(panel.display(track.modelData.sample_rate))
                                    .arg(panel.display(track.modelData.channels))
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Add preview to preparation")
                enabled: panel.file.ready === true && !panel.workspace.selectedInSet
                onClicked: panel.workspace.addCurrent()
            }
        }
    }
}
