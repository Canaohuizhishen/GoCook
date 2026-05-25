import QtQuick
import QtQuick.Controls
import client
import "."

Rectangle {
    id: root
    width: chipText.implicitWidth + 12
    height: 24
    radius: 12
    color: Theme.primaryLightColor

    property string text: ""

    Text {
        id: chipText
        anchors.centerIn: parent
        text: root.text
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeSmall
        color: "white"
    }
}
