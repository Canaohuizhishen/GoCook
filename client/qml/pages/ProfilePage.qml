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

            Canvas {
                Layout.alignment: Qt.AlignHCenter
                width: 48
                height: 48
                property color circleColor: Theme.textHint
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.strokeStyle = circleColor
                    ctx.lineWidth = 1.5
                    ctx.beginPath()
                    ctx.arc(width / 2, height / 2, width / 2 - 2, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: authViewModel.loggedIn ? qsTr("欢迎, %1").arg(authViewModel.username) : qsTr("未登录")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH2
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: authViewModel.loggedIn ? qsTr("用户 ID: %1").arg(authViewModel.userId) : ""
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
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
            buttonText: qsTr("+ 发布菜谱")
            buttonType: CustomButton.ButtonType.Primary
            visible: authViewModel.loggedIn
            onClicked: showSubmitRequest()
        }

        Item { Layout.fillHeight: true }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXSmall
            visible: authViewModel.loggedIn

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.dividerColor
            }

            Text {
                text: qsTr("主题模式")
                font { family: Theme.fontFamily; pointSize: Theme.fontSizeBody; weight: Theme.fontWeightBold }
                color: Theme.textPrimary
                Layout.topMargin: Theme.spacingSmall
            }

            RadioButton {
                text: qsTr("跟随系统")
                checked: Theme.themeMode === Theme.themeModeSystem
                onClicked: Theme.themeMode = Theme.themeModeSystem
                Layout.fillWidth: true
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
            RadioButton {
                text: qsTr("浅色")
                checked: Theme.themeMode === Theme.themeModeLight
                onClicked: Theme.themeMode = Theme.themeModeLight
                Layout.fillWidth: true
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
            RadioButton {
                text: qsTr("深色")
                checked: Theme.themeMode === Theme.themeModeDark
                onClicked: Theme.themeMode = Theme.themeModeDark
                Layout.fillWidth: true
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: parent.indicator.width + parent.spacing
                }
            }
        }

        CustomButton {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingMedium
            buttonText: qsTr("退出登录")
            buttonType: CustomButton.ButtonType.Secondary
            visible: authViewModel.loggedIn
            onClicked: authViewModel.logout()
        }
    }
}
