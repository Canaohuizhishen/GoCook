import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client

Rectangle {
    id: root
    height: 32
    color: Theme.dividerColor
    radius: Theme.radiusSmall
    implicitWidth: headerRow.implicitWidth + Theme.spacingSmall * 2

    property real nameWidth: 80
    property real reqWidth: 50
    property real invWidth: 50
    property real buyWidth: 50
    property real unitWidth: 40

    Row {
        id: headerRow
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Text {
            width: root.nameWidth; height: 32
            text: qsTr("食材")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            font.bold: true
            color: Theme.textSecondary
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            width: root.reqWidth; height: 32
            text: qsTr("需购")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            font.bold: true
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            width: root.invWidth; height: 32
            text: qsTr("库存")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            font.bold: true
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            width: root.buyWidth; height: 32
            text: qsTr("建议买")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            font.bold: true
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            width: root.unitWidth; height: 32
            text: qsTr("单位")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            font.bold: true
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            width: 40; height: 32
            text: qsTr("状态")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            font.bold: true
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
