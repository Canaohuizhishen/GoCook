import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "."

Dialog {
    id: root
    modal: true
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.8, 320)

    property var groupsModel: []
    signal groupSelected(int groupId)

    background: Rectangle {
        radius: Theme.radiusMedium
        color: Theme.cardBackground
        border.color: Theme.dividerColor
    }

    Column {
        width: parent.width
        spacing: Theme.spacingMedium
        topPadding: Theme.spacingMedium
        bottomPadding: Theme.spacingMedium

        Text {
            text: qsTr("选择收藏分组")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH3
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Item { width: 1; height: 1 }

        Repeater {
            id: groupRepeater
            width: parent.width
            model: root.groupsModel

            Rectangle {
                width: parent.width
                height: 44
                radius: Theme.radiusSmall
                color: groupHovered ? Theme.searchBarBackground : "transparent"
                property bool groupHovered: false

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMedium
                    anchors.rightMargin: Theme.spacingMedium
                    spacing: Theme.spacingSmall

                    Text {
                        text: "\u2606"
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.accentColor
                    }

                    Text {
                        text: modelData.name || ""
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: "(" + (modelData.count || 0) + ")"
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.textHint
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: parent.groupHovered = true
                    onExited: parent.groupHovered = false
                    onClicked: {
                        root.groupSelected(modelData.id)
                        root.close()
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.dividerColor
                    opacity: 0.3
                }
            }
        }

        // 取消按钮
        Rectangle {
            width: parent.width
            height: 44
            radius: Theme.radiusMedium
            color: "transparent"
            border.color: Theme.dividerColor
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: qsTr("取消")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: Theme.textSecondary
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.close()
            }
        }
    }
}
