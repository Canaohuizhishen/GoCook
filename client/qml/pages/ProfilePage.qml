import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("个人中心")

    signal showSubmitRequest()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingMedium

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingSmall

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: authViewModel.loggedIn ? qsTr("欢迎, %1").arg(authViewModel.username) : qsTr("未登录")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeH2
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: authViewModel.loggedIn ? qsTr("用户 ID: %1").arg(authViewModel.userId) : ""
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textHint
                visible: authViewModel.loggedIn
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.dividerColor
        }

        CustomButton {
            Layout.fillWidth: true
            buttonText: qsTr("+ 提交新菜谱")
            buttonType: CustomButton.ButtonType.Primary
            visible: authViewModel.loggedIn
            onClicked: showSubmitRequest()
        }

        Item { Layout.fillHeight: true }

        CustomButton {
            Layout.fillWidth: true
            buttonText: qsTr("退出登录")
            buttonType: CustomButton.ButtonType.Secondary
            visible: authViewModel.loggedIn
            onClicked: authViewModel.logout()
        }
    }
}
