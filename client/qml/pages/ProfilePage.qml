import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("个人中心")

    signal showSubmitRequest()
    signal showProfileEditRequest()

    // 页面创建时和每次可见时都加载最新用户资料
    Component.onCompleted: authViewModel.loadProfile()
    onVisibleChanged: {
        if (visible) authViewModel.loadProfile()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingMedium

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingSmall

            // ★ 圆形头像（Canvas 绘制，不依赖额外模块）
            Item {
                Layout.alignment: Qt.AlignHCenter
                width: 72; height: 72

                // 背景圆
                Rectangle {
                    anchors.fill: parent; radius: width / 2
                    color: Theme.cardBackground
                    border.color: Theme.dividerColor; border.width: 1
                }

                // 无头像时的占位文字
                Text {
                    id: profilePlaceholder
                    anchors.centerIn: parent
                    text: {
                        var n = authViewModel.profileDisplayName
                        if (n.length > 0) return n.charAt(0).toUpperCase()
                        n = authViewModel.username
                        if (n.length > 0) return n.charAt(0).toUpperCase()
                        return "G"
                    }
                    font.family: Theme.fontFamily; font.pointSize: 32
                    font.weight: Theme.fontWeightMedium; color: Theme.textHint
                }

                // Canvas 绘制圆形头像
                Canvas {
                    id: profileCanvas
                    anchors.fill: parent; anchors.margins: 2

                    property url pendingUrl: ""

                    onPendingUrlChanged: {
                        if (pendingUrl.toString().length > 0)
                            loadImage(pendingUrl)
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        var src = pendingUrl.toString()
                        if (src.length === 0 || !ctx.isImageReady(src)) {
                            profilePlaceholder.visible = true
                            return
                        }
                        profilePlaceholder.visible = false
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2, 0, Math.PI * 2)
                        ctx.closePath()
                        ctx.clip()
                        ctx.drawImage(src, 0, 0, width, height)
                    }

                    onImageLoaded: requestPaint()
                }

                // 监听 profileAvatarUrl 变化
                Connections {
                    target: authViewModel
                    function onProfileChanged() {
                        var url = authViewModel.profileAvatarUrl
                        if (url.length > 0)
                            profileCanvas.pendingUrl = "http://127.0.0.1:8080" + url
                        else
                            profileCanvas.pendingUrl = ""
                    }
                }

                // 圆形边框
                Rectangle {
                    anchors.fill: parent; radius: width / 2
                    color: "transparent"
                    border.color: Theme.dividerColor; border.width: 1.5
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
            buttonText: qsTr("编辑个人资料")
            buttonType: CustomButton.ButtonType.Secondary
            visible: authViewModel.loggedIn
            onClicked: showProfileEditRequest()
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
