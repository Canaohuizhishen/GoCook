import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client

Dialog {
    id: root
    modal: true
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    width: Math.min(parent.width * 0.8, 320)

    property string dialogTitle: qsTr("输入")
    property string placeholderText: qsTr("请输入...")
    property string confirmText: qsTr("确认")
    property string cancelText: qsTr("取消")
    property string inputText: ""

    signal confirmed(string text)
    signal cancelled()

    onVisibleChanged: {
        if (visible) {
            inputField.text = root.inputText
            inputField.forceActiveFocus()
        }
    }

    background: Rectangle {
        radius: Theme.radiusMedium
        color: Theme.cardBackground
        border.color: Theme.dividerColor
    }

    Column {
        spacing: Theme.spacingMedium
        width: parent.width

        Text {
            text: root.dialogTitle
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH3
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }

        TextField {
            id: inputField
            width: parent.width
            height: 44
            text: root.inputText
            placeholderText: root.placeholderText
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            color: Theme.textPrimary
            verticalAlignment: TextInput.AlignVCenter
            background: Rectangle {
                radius: Theme.radiusMedium
                color: Theme.cardBackground
                border.color: Theme.dividerColor
            }
            onAccepted: {
                if (text.trim().length > 0) {
                    root.confirmed(text.trim())
                    root.close()
                }
            }
        }

        RowLayout {
            width: parent.width
            spacing: Theme.spacingMedium

            Button {
                Layout.fillWidth: true
                height: 40
                text: root.cancelText
                flat: true
                background: Rectangle {
                    radius: Theme.radiusMedium
                    color: parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.15)
                         : parent.hovered ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.06)
                         : "transparent"
                    border.color: parent.down ? Theme.primaryDarkColor
                                : parent.hovered ? Theme.primaryColor
                                : Theme.dividerColor
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    root.cancelled()
                    root.close()
                }
            }

            Button {
                Layout.fillWidth: true
                height: 40
                text: root.confirmText
                background: Rectangle {
                    radius: Theme.radiusMedium
                    color: parent.down ? Theme.primaryDarkColor
                         : parent.hovered ? Theme.primaryLightColor
                         : Theme.primaryColor
                    Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (inputField.text.trim().length > 0) {
                        root.confirmed(inputField.text.trim())
                        root.close()
                    }
                }
            }
        }
    }
}
