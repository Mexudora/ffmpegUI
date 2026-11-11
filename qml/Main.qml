import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 960
    height: 640
    minimumWidth: 640
    minimumHeight: 400
    visible: true
    title: qsTr("ffmpegUI")

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(window.width - 64, 560)
        spacing: 16

        Label {
            Layout.fillWidth: true
            text: qsTr("ffmpegUI")
            font.pixelSize: 32
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("A graphical interface for FFmpeg")
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Project scaffold. Conversion settings and the task queue will be added later.")
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
