import QtQuick
import client

Text {
    id: root
    width: parent ? parent.width : implicitWidth
    text: ""
    font.family: Theme.fontFamily
    font.pointSize: Theme.fontSizeH3
    font.weight: Theme.fontWeightMedium
    color: Theme.textPrimary

    property alias headerText: root.text
}
