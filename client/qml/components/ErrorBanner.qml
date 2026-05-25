import QtQuick
import QtQuick.Controls
import client
import "."

Rectangle {
    id: root
    width: parent ? parent.width : 200
    height: 32
    color: "#E74C3C"
    visible: text.length > 0

    property string text: ""
    property int autoDismissMs: 3000

    signal dismissed()

    Timer {
        id: timer
        interval: root.autoDismissMs
        onTriggered: {
            root.text = ""
            root.dismissed()
        }
    }

    onTextChanged: {
        if (text.length > 0) {
            timer.restart()
        }
    }

    Text {
        anchors.centerIn: parent
        text: root.text
        color: "white"
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeCaption
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }
}
