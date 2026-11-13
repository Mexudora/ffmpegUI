import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FFmpegUI

ColumnLayout {
    id: form
    property alias inputFile: conversion.inputFile
    property real durationSeconds: 0
    required property ConversionQueue queue
    spacing: 12

    ConversionCommand {
        id: conversion
    }

    Connections {
        target: form.queue.runner
        function onCompleted(state) {
            // Recheck the current draft; never restore settings from the completed job.
            conversion.refresh()
        }
    }

    FileDialog {
        id: outputDialog
        title: qsTr("Choose an output file")
        fileMode: FileDialog.SaveFile
        // Existing destinations are rejected by ConversionCommand.
        options: FileDialog.DontConfirmOverwrite
        defaultSuffix: conversion.container
        nameFilters: conversion.container === "mp4"
                     ? [qsTr("MP4 files (*.mp4)")]
                     : [qsTr("MKV files (*.mkv)")]
        onAccepted: conversion.outputFile = selectedFile
    }

    MenuSeparator {
        Layout.fillWidth: true
    }

    Label {
        text: qsTr("Conversion settings")
        font.pixelSize: 24
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("The output will contain the first video stream (excluding cover art) and the first audio stream, when present.")
        wrapMode: Text.WordWrap
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 16
        rowSpacing: 12

        Label { text: qsTr("Container") }
        ComboBox {
            Layout.fillWidth: true
            model: ["MP4", "MKV"]
            onActivated: conversion.container = currentIndex === 0 ? "mp4" : "mkv"
        }

        Label { text: qsTr("Video") }
        ComboBox {
            Layout.fillWidth: true
            model: [qsTr("H.264 (libx264)"), qsTr("Copy without re-encoding")]
            onActivated: conversion.videoCodec = currentIndex === 0 ? "libx264" : "copy"
        }

        Label { text: qsTr("Audio") }
        ComboBox {
            Layout.fillWidth: true
            model: ["AAC", qsTr("Copy without re-encoding")]
            onActivated: conversion.audioCodec = currentIndex === 0 ? "aac" : "copy"
        }

        Label { text: qsTr("Audio bitrate, kbit/s") }
        ComboBox {
            Layout.fillWidth: true
            model: [64, 96, 128, 160, 192, 256, 320]
            currentIndex: 4
            enabled: conversion.audioCodec !== "copy"
            onActivated: conversion.audioBitrateKbps = Number(currentText)
        }
    }

    Label {
        Layout.fillWidth: true
        visible: conversion.videoCodec === "copy" || conversion.audioCodec === "copy"
        text: qsTr("Copying requires the source codec to be supported by the selected container.")
        wrapMode: Text.WordWrap
    }

    Label {
        text: qsTr("Output file")
    }

    RowLayout {
        Layout.fillWidth: true

        TextField {
            Layout.fillWidth: true
            id: outputField
            selectByMouse: true
            onTextEdited: conversion.outputPath = text

            Binding {
                target: outputField
                property: "text"
                value: conversion.outputPath
                when: !outputField.activeFocus
                restoreMode: Binding.RestoreNone
            }
            placeholderText: qsTr("Edit the output path or choose a file")
        }

        Button {
            text: qsTr("Choose…")
            onClicked: outputDialog.open()
        }
    }

    Label {
        Layout.fillWidth: true
        visible: conversion.error.length > 0
        text: conversion.error
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
    }

    Label {
        text: qsTr("Command for PowerShell")
        font.bold: true
    }

    TextArea {
        Layout.fillWidth: true
        Layout.preferredHeight: 140
        readOnly: true
        selectByMouse: true
        wrapMode: TextEdit.WrapAnywhere
        textFormat: TextEdit.PlainText
        text: conversion.commandPreview
        placeholderText: qsTr("The command will appear after the settings are valid")
    }

    Button {
        text: qsTr("Copy command")
        enabled: conversion.valid
        onClicked: conversion.copyPreview()
    }

    Button {
        text: qsTr("Add job to queue")
        enabled: conversion.valid
        onClicked: form.queue.enqueue(conversion, form.durationSeconds)
    }

    Label {
        Layout.fillWidth: true
        visible: form.queue.error.length > 0
        text: form.queue.error
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
    }
}
