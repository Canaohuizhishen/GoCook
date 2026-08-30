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
    width: Math.min(parent.width * 0.85, 340)

    property string dialogTitle: qsTr("确认")
    property string message: qsTr("确定要执行此操作吗？")
    property string confirmText: qsTr("确定")
    property string cancelText: qsTr("取消")
    property color confirmColor: Theme.primaryColor

    signal confirmed()
    signal cancelled()

    background: Rectangle {
        color: Theme.cardBackground
        radius: Theme.radiusMedium
        border.color: Theme.dividerColor
        border.width: 1
    }

    ColumnLayout {
        spacing: Theme.spacingMedium
        width: parent.width

        Text {
            text: root.dialogTitle
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH3
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }

        Text {
            text: root.message
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Theme.spacingSmall
            Layout.fillWidth: true

            CustomButton {
                Layout.fillWidth: true
                buttonText: root.cancelText
                buttonType: CustomButton.ButtonType.Secondary
                onClicked: {
                    root.cancelled()
                    root.close()
                }
            }

            CustomButton {
                Layout.fillWidth: true
                buttonText: root.confirmText
                buttonColor: root.confirmColor
                buttonType: CustomButton.ButtonType.Primary
                onClicked: {
                    root.confirmed()
                    root.close()
                }
            }
        }
    }
}
