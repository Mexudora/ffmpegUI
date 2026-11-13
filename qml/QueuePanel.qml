import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFmpegUI

ColumnLayout {
    id: panel
    required property ConversionQueue queue
    spacing: 12

    MenuSeparator { Layout.fillWidth: true }
    Label {
        text: qsTr("Conversion queue")
        font.pixelSize: 24
    }
    Label {
        text: qsTr("Jobs: %1; waiting: %2").arg(panel.queue.count).arg(panel.queue.pendingCount)
    }

    RowLayout {
        Button {
            text: qsTr("Start queue")
            enabled: !panel.queue.running && !panel.queue.busy && panel.queue.pendingCount > 0
            onClicked: panel.queue.start()
        }
        Button {
            text: qsTr("Cancel current job")
            enabled: panel.queue.busy && !panel.queue.runner.cancelling
            onClicked: panel.queue.cancelCurrent()
        }
    }
    RowLayout {
        Button {
            text: qsTr("Stop after current job")
            enabled: panel.queue.running && !panel.queue.stopRequested
            onClicked: panel.queue.requestStopAfterCurrent()
        }
        Button {
            text: qsTr("Clear finished jobs")
            onClicked: panel.queue.clearFinished()
        }
    }
    Label {
        Layout.fillWidth: true
        visible: panel.queue.stopRequested
        text: qsTr("The queue will stop after the current job.")
        wrapMode: Text.WordWrap
    }

    ListView {
        id: jobList
        Layout.fillWidth: true
        Layout.preferredHeight: 300
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
        Layout.preferredHeight: 180
        TextArea {
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            textFormat: TextEdit.PlainText
            text: panel.queue.selectedLog
        }
    }
}
