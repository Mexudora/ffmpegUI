import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FFmpegUI

ApplicationWindow {
    id: window

    width: 960
    height: 640
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: qsTr("ffmpegUI")

    property url selectedMediaFile

    function valueOrUnknown(value) {
        return value === undefined || value === null || value === ""
                ? qsTr("Unknown") : String(value)
    }

    function durationText(value) {
        const seconds = Number(value)
        if (value === undefined || value === null || value === ""
                || !isFinite(seconds) || seconds < 0)
            return qsTr("Unknown")
        return qsTr("%1 s").arg(seconds.toFixed(3))
    }

    function frameRateText(value) {
        if (typeof value !== "string")
            return qsTr("Unknown")
        const parts = value.split("/")
        if (parts.length !== 2)
            return qsTr("Unknown")
        const numerator = Number(parts[0])
        const denominator = Number(parts[1])
        if (!isFinite(numerator) || !isFinite(denominator)
                || numerator <= 0 || denominator <= 0)
            return qsTr("Unknown")
        return qsTr("%1 fps").arg((numerator / denominator).toFixed(3))
    }

    MediaProbe {
        id: mediaProbe
        executablePath: executableField.text
    }

    FileDialog {
        id: mediaDialog
        title: qsTr("Select a media file")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            window.selectedMediaFile = selectedFile
            mediaProbe.probe(selectedFile)
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 16

            Label {
                text: qsTr("Media information")
                font.pixelSize: 26
            }

            Label {
                text: qsTr("Full path to ffprobe (without quotes)")
            }

            TextField {
                id: executableField
                Layout.fillWidth: true
                // Development default; the user can still select another installation.
                text: "C:/ffmpeg/bin/ffprobe.exe"
                placeholderText: qsTr("For example: C:/FFmpeg/bin/ffprobe.exe")
                enabled: !mediaProbe.busy
                selectByMouse: true
            }

            RowLayout {
                Button {
                    text: qsTr("Select media file…")
                    enabled: !mediaProbe.busy && executableField.text.trim().length > 0
                    onClicked: mediaDialog.open()
                }

                Button {
                    text: qsTr("Analyze again")
                    enabled: !mediaProbe.busy
                             && executableField.text.trim().length > 0
                             && window.selectedMediaFile.toString().length > 0
                    onClicked: mediaProbe.probe(window.selectedMediaFile)
                }

                BusyIndicator {
                    running: mediaProbe.busy
                    visible: running
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                }
            }

            Label {
                Layout.fillWidth: true
                visible: mediaProbe.sourcePath.length > 0
                text: mediaProbe.sourcePath
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
            }

            Label {
                Layout.fillWidth: true
                visible: mediaProbe.busy
                text: qsTr("Analyzing file…")
            }

            Label {
                Layout.fillWidth: true
                visible: mediaProbe.error.length > 0
                text: mediaProbe.error
                textFormat: Text.PlainText
                color: "#b3261e"
                wrapMode: Text.WrapAnywhere
            }

            ConversionForm {
                Layout.fillWidth: true
                visible: mediaProbe.metadata.format !== undefined
                enabled: !mediaProbe.busy
                inputFile: window.selectedMediaFile
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: mediaProbe.metadata.format !== undefined
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Container: %1").arg(window.valueOrUnknown(
                        mediaProbe.metadata.format
                        ? mediaProbe.metadata.format.format_name : undefined))
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                }

                Label {
                    text: qsTr("Duration: %1").arg(window.durationText(
                        mediaProbe.metadata.format
                        ? mediaProbe.metadata.format.duration : undefined))
                }

                Label {
                    text: qsTr("Streams")
                    font.bold: true
                }

                Repeater {
                    model: mediaProbe.metadata.streams || []

                    delegate: Frame {
                        id: streamFrame
                        required property var modelData
                        Layout.fillWidth: true

                        ColumnLayout {
                            anchors.fill: parent

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Stream #%1 — %2 — %3")
                                    .arg(window.valueOrUnknown(streamFrame.modelData.index))
                                    .arg(window.valueOrUnknown(streamFrame.modelData.codec_type))
                                    .arg(window.valueOrUnknown(streamFrame.modelData.codec_name))
                                textFormat: Text.PlainText
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: streamFrame.modelData.codec_type === "video"
                                text: qsTr("Resolution: %1 × %2; average frame rate: %3")
                                    .arg(window.valueOrUnknown(streamFrame.modelData.width))
                                    .arg(window.valueOrUnknown(streamFrame.modelData.height))
                                    .arg(window.frameRateText(streamFrame.modelData.avg_frame_rate))
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: streamFrame.modelData.codec_type === "audio"
                                text: qsTr("Sample rate: %1 Hz; channels: %2")
                                    .arg(window.valueOrUnknown(streamFrame.modelData.sample_rate))
                                    .arg(window.valueOrUnknown(streamFrame.modelData.channels))
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
