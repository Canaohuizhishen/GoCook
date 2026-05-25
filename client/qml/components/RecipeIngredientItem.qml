import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "."

Rectangle {
    id: root
    width: parent ? parent.width : 200
    height: 28
    color: "transparent"

    property string name: ""
    property real quantity: 0
    property string unit: ""
    property bool showDivider: true

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacingSmall

        Text {
            text: "\u2022"
            font.pointSize: Theme.fontSizeBody
            color: Theme.primaryColor
            font.bold: true
        }

        Text {
            text: root.name
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            color: Theme.textPrimary
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Text {
            text: root.quantity + " " + root.unit
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            color: Theme.textHint
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.dividerColor
        opacity: 0.3
        visible: root.showDivider
    }
}
