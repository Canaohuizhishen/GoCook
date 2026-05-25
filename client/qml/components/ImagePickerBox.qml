import QtQuick
import QtQuick.Controls
import client
import "."

Rectangle {
    id: root
    width: parent ? parent.width : 200
    height: 160
    radius: Theme.radiusMedium
    color: Theme.cardBackground
    border.color: Theme.dividerColor
    border.width: 1

    property alias source: preview.source
    property string imagePath: ""
    property string placeholderText: qsTr("点击选择封面图片")

    signal imageSelected(string path)

    Image {
        id: preview
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }

    Text {
        anchors.centerIn: parent
        text: root.placeholderText
        color: Theme.textHint
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeBody
        visible: preview.source === "" || preview.status !== Image.Ready
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: picker.open()
    }

    NativeFileDialog {
        id: picker
        onFileSelected: function(path) {
            root.imagePath = path
            root.source = "file:///" + path
            root.imageSelected(path)
        }
    }
}
