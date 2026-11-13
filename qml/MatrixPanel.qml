import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FFmpegUI

Frame {
    id: panel
    required property MediaWorkspace workspace
    required property ConversionQueue queue
    readonly property int columnWidth: 230
    readonly property int controlsWidth: 300
    readonly property int headerHeight: 84
    readonly property int rowHeight: 110
    readonly property int previewHeight: 48
    readonly property real matrixHeight: headerHeight + workspace.rows.length * rowHeight + previewHeight + 18
    property bool enqueueAfterPick: false
    padding: 10

    function requestEnqueue() {
        if (workspace.outputPath.trim().length === 0) {
            enqueueAfterPick = true
            outputDirectoryDialog.open()
        } else {
            workspace.enqueue(queue)
        }
    }
    FolderDialog {
        id: outputDirectoryDialog
        title: qsTr("Choose output directory")
        onAccepted: {
            panel.workspace.chooseOutputDirectory(selectedFolder)
            if (panel.enqueueAfterPick) panel.workspace.enqueue(panel.queue)
            panel.enqueueAfterPick = false
        }
        onRejected: panel.enqueueAfterPick = false
    }
    FileDialog {
        id: outputFileDialog
        title: qsTr("Choose output file")
        fileMode: FileDialog.SaveFile
        options: FileDialog.DontConfirmOverwrite
        defaultSuffix: panel.workspace.container
        nameFilters: panel.workspace.container === "mp4" ? ["MP4 (*.mp4)"] : ["MKV (*.mkv)"]
        onAccepted: panel.workspace.chooseOutputFile(selectedFile)
    }
    CommandPreviewWindow {
        id: preview
        transientParent: panel.Window.window
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Label {
            text: qsTr("3. Tracks and conversion settings")
            font.bold: true
        }
        Item {
            id: matrixArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            clip: true
            Label {
                anchors.centerIn: parent
                visible: panel.workspace.count === 0
                text: qsTr("Add files to prepare conversion jobs")
            }
            Flickable {
                id: vertical
                anchors.fill: parent
                visible: panel.workspace.count > 0
                clip: true
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: panel.matrixHeight
                ScrollBar.vertical: ScrollBar {}
                Row {
                    width: vertical.width
                    height: panel.matrixHeight
                    spacing: 8
                    Flickable {
                        id: fileFlick
                        width: Math.max(0, vertical.width - panel.controlsWidth - 8)
                        height: panel.matrixHeight
                        clip: true
                        flickableDirection: Flickable.HorizontalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        contentWidth: Math.max(width, fileColumns.width)
                        contentHeight: height
                        ScrollBar.horizontal: ScrollBar {
                            parent: matrixArea
                            x: 0
                            y: matrixArea.height - height
                            width: fileFlick.width
                            policy: ScrollBar.AsNeeded
                            z: 3
                        }
                        Row {
                            id: fileColumns
                            spacing: 8
                            Repeater {
                                model: panel.workspace.files
                                delegate: Rectangle {
                                    id: fileColumn
                                    required property var modelData
                                    readonly property string fileId: modelData.id
                                    width: panel.columnWidth
                                    height: panel.matrixHeight
                                    color: fileId === panel.workspace.selectedId
                                           ? Qt.tint(panel.palette.base, Qt.rgba(panel.palette.highlight.r,
                                               panel.palette.highlight.g, panel.palette.highlight.b, 0.18))
                                           : panel.palette.base
                                    border.color: fileId === panel.workspace.selectedId
                                                  ? panel.palette.highlight : panel.palette.mid
                                    Column {
                                        width: parent.width
                                        Item {
                                            width: parent.width
                                            height: panel.headerHeight
                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    Button {
                                                        Layout.fillWidth: true
                                                        Layout.minimumWidth: 0
                                                        text: fileColumn.modelData.name
                                                        flat: true
                                                        clip: true
                                                        onClicked: panel.workspace.selectFile(fileColumn.fileId)
                                                    }
                                                    ToolButton {
                                                        text: "×"
                                                        onClicked: panel.workspace.removeFile(fileColumn.fileId)
                                                    }
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: fileColumn.modelData.loading ? qsTr("Analyzing…")
                                                          : fileColumn.modelData.error || ""
                                                    color: fileColumn.modelData.error ? "#b3261e" : panel.palette.text
                                                    textFormat: Text.PlainText
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                        Repeater {
                                            model: panel.workspace.rows
                                            delegate: Item {
                                                id: cellItem
                                                required property var modelData
                                                readonly property var cell: modelData.cells[fileColumn.fileId] || null
                                                width: fileColumn.width
                                                height: panel.rowHeight
                                                ColumnLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 6
                                                    CheckBox {
                                                        Layout.fillWidth: true
                                                        Layout.minimumWidth: 0
                                                        clip: true
                                                        text: cellItem.cell ? qsTr("Stream 0:%1 — %2")
                                                            .arg(cellItem.cell.index).arg(cellItem.cell.codec_name || "") : ""
                                                        checked: cellItem.cell ? cellItem.cell.checked : false
                                                        visible: cellItem.cell !== null
                                                        onClicked: panel.workspace.setIncluded(cellItem.modelData.id,
                                                            fileColumn.fileId, checked)
                                                    }
                                                    Label {
                                                        Layout.fillWidth: true
                                                        visible: cellItem.cell !== null
                                                        text: cellItem.cell ? (cellItem.cell.title || "") + "\n"
                                                              + (cellItem.cell.language || "") : ""
                                                        textFormat: Text.PlainText
                                                        elide: Text.ElideRight
                                                        maximumLineCount: 2
                                                        wrapMode: Text.WordWrap
                                                    }
                                                    Item { Layout.fillHeight: true }
                                                }
                                            }
                                        }
                                        Item {
                                            width: parent.width
                                            height: panel.previewHeight
                                            Button {
                                                anchors.centerIn: parent
                                                text: qsTr("View command")
                                                enabled: fileColumn.modelData.ready
                                                onClicked: {
                                                    preview.commandText = panel.workspace.commandPreview(fileColumn.fileId)
                                                    preview.show()
                                                    preview.raise()
                                                    preview.requestActivate()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Column {
                        width: panel.controlsWidth
                        Item {
                            width: parent.width
                            height: panel.headerHeight
                            Label {
                                anchors.fill: parent
                                anchors.margins: 8
                                text: qsTr("Shared settings for each track row")
                                font.bold: true
                                wrapMode: Text.WordWrap
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Repeater {
                            model: panel.workspace.rows
                            delegate: Item {
                                id: controls
                                required property var modelData
                                width: panel.controlsWidth
                                height: panel.rowHeight
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    CheckBox {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        clip: true
                                        text: controls.modelData.label
                                        tristate: true
                                        checkState: controls.modelData.checkState
                                        nextCheckState: function() {
                                            return checkState === Qt.Checked ? Qt.Unchecked : Qt.Checked
                                        }
                                        onClicked: panel.workspace.setRowIncluded(controls.modelData.id,
                                            checkState === Qt.Checked)
                                        ToolTip.visible: hovered
                                        ToolTip.text: controls.modelData.label + "\n" + controls.modelData.matchNote
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        ComboBox {
                                            Layout.fillWidth: true
                                            model: controls.modelData.codecOptions
                                            textRole: "label"
                                            valueRole: "value"
                                            currentIndex: {
                                                const options = controls.modelData.codecOptions
                                                for (let i = 0; i < options.length; ++i)
                                                    if (options[i].value === controls.modelData.codec) return i
                                                return -1
                                            }
                                            onActivated: panel.workspace.setRowCodec(controls.modelData.id, currentValue)
                                        }
                                        ComboBox {
                                            Layout.preferredWidth: 95
                                            visible: controls.modelData.type === "audio"
                                            enabled: controls.modelData.codec !== "copy"
                                            model: [64, 96, 128, 160, 192, 256, 320]
                                            currentIndex: [64, 96, 128, 160, 192, 256, 320].indexOf(controls.modelData.bitrate)
                                            onActivated: panel.workspace.setRowBitrate(controls.modelData.id, Number(currentText))
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Audio bitrate, kbit/s")
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: controls.modelData.matchNote
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        // Fixed footer: none of these controls belongs to either flickable.
        RowLayout {
            Label { text: qsTr("Container") }
            ComboBox {
                model: ["MP4", "MKV"]
                currentIndex: panel.workspace.container === "mp4" ? 0 : 1
                onActivated: panel.workspace.container = currentIndex === 0 ? "mp4" : "mkv"
            }
            ComboBox {
                model: [qsTr("Directory"), qsTr("File")]
                currentIndex: panel.workspace.outputIsDirectory ? 0 : 1
                enabled: panel.workspace.count <= 1
                onActivated: panel.workspace.outputIsDirectory = currentIndex === 0
            }
            Label { text: qsTr("Files: %1").arg(panel.workspace.count) }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: outputField
                Layout.fillWidth: true
                selectByMouse: true
                placeholderText: qsTr("Output directory or file; leave empty to choose a directory")
                onTextEdited: panel.workspace.outputPath = text
                Binding {
                    target: outputField
                    property: "text"
                    value: panel.workspace.outputPath
                    when: !outputField.activeFocus
                    restoreMode: Binding.RestoreNone
                }
            }
            Button {
                text: qsTr("Choose…")
                onClicked: {
                    panel.enqueueAfterPick = false
                    if (panel.workspace.outputIsDirectory) outputDirectoryDialog.open()
                    else outputFileDialog.open()
                }
            }
        }
        RowLayout {
            Button {
                text: qsTr("Add set to queue")
                enabled: panel.workspace.count > 0 && !panel.workspace.busy
                onClicked: panel.requestEnqueue()
            }
            Button {
                text: qsTr("Clear preparation")
                enabled: panel.workspace.count > 0
                onClicked: panel.workspace.clearSet()
            }
            Label {
                Layout.fillWidth: true
                text: panel.workspace.error || panel.workspace.message
                textFormat: Text.PlainText
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                color: panel.workspace.error.length > 0 ? "#b3261e" : panel.palette.text
                ToolTip.visible: errorHover.hovered
                ToolTip.text: text
                HoverHandler { id: errorHover }
            }
        }
    }
}
