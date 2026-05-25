import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "."

RowLayout {
    id: root
    width: parent ? parent.width : 200
    spacing: Theme.spacingSmall
    layoutDirection: Qt.LeftToRight

    property int stepNumber: 1
    property string description: ""
    property color numberColor: Theme.primaryColor

    Rectangle {
        Layout.preferredWidth: 20
        Layout.preferredHeight: 20
        radius: 10
        color: root.numberColor

        Text {
            anchors.centerIn: parent
            text: root.stepNumber
            font.pointSize: Theme.fontSizeSmall - 1
            font.bold: true
            color: Theme.textOnPrimary
        }
    }

    Text {
        Layout.fillWidth: true
        text: root.description
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeBody
        color: Theme.textPrimary
        wrapMode: Text.WordWrap
        Layout.maximumWidth: parent ? parent.width - 40 : 200
    }
}
