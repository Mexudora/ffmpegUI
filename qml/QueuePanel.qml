import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFmpegUI

Frame {
    id: panel
    required property ConversionQueue queue
    padding: 10
    ColumnLayout {
        anchors.fill: parent
        spacing: 6
        Label { text: qsTr("2. Conversion queue"); font.bold: true }
        Label {
            text: qsTr("Jobs: %1; waiting: %2").arg(panel.queue.count).arg(panel.queue.pendingCount)
        }
        ListView {
            id: jobList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            clip: true
            spacing: 8
            model: panel.queue
            ScrollBar.vertical: ScrollBar {}

            delegate: Frame {
                id: jobRow
                required property string jobId
                required property string inputPath
                required property string outputPath
                required property int jobState
                required property string statusText
                required property real jobProgress
                required property string jobError
                width: jobList.width

                contentItem: ColumnLayout {
                    Label {
                        Layout.fillWidth: true
                        text: jobRow.inputPath
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Output: %1").arg(jobRow.outputPath)
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.fillWidth: true
                        text: jobRow.statusText
                        wrapMode: Text.WordWrap
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: jobRow.jobState === ConversionQueue.Starting
                                 || jobRow.jobState === ConversionQueue.Running
                                 || jobRow.jobState === ConversionQueue.Cancelling
                        from: 0
                        to: 1
                        value: Math.max(0, jobRow.jobProgress)
                        indeterminate: jobRow.jobProgress < 0
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: jobRow.jobError.length > 0
                        text: jobRow.jobError
                        textFormat: Text.PlainText
                        color: "#b3261e"
                        wrapMode: Text.WrapAnywhere
                    }
                    RowLayout {
                        Button {
                            text: qsTr("Show log")
                            onClicked: panel.queue.selectJob(jobRow.jobId)
                        }
                        Button {
                            text: qsTr("Remove waiting job")
                            visible: jobRow.jobState === ConversionQueue.Waiting
                            onClicked: panel.queue.removeWaiting(jobRow.jobId)
                        }
                    }
                }
            }
        }

        Label {
            text: qsTr("Log of the selected job")
            font.bold: true
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(55, panel.height * 0.18)
            TextArea {
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                textFormat: TextEdit.PlainText
                text: panel.queue.selectedLog
            }
        }
        Label {
            Layout.fillWidth: true
            visible: panel.queue.stopRequested
            text: qsTr("The queue will stop after the current job.")
            wrapMode: Text.WordWrap
        }
        // Fixed actions, outside the list and the log scroll areas.
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            Button {
                Layout.fillWidth: true
                text: qsTr("Start queue")
                enabled: !panel.queue.running && !panel.queue.busy && panel.queue.pendingCount > 0
                onClicked: panel.queue.start()
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Cancel current job")
                enabled: panel.queue.busy && !panel.queue.runner.cancelling
                onClicked: panel.queue.cancelCurrent()
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Stop after current job")
                enabled: panel.queue.running && !panel.queue.stopRequested
                onClicked: panel.queue.requestStopAfterCurrent()
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Clear finished jobs")
                onClicked: panel.queue.clearFinished()
            }
        }
    }
}
