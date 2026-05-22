import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    title: qsTr("个人中心")

    signal showSubmitRequest()
    signal showProfileEditRequest()
    signal showAccountSecurityRequest()
    signal showNotificationRequest()

    // 页面创建时和每次可见时都加载最新用户资料 + 通知未读数
    Component.onCompleted: {
        authViewModel.loadProfile()
        notifyVM.loadNotifications(1, 20)
    }
    onVisibleChanged: {
        if (visible) {
            authViewModel.loadProfile()
            notifyVM.loadNotifications(1, 20)
        }
    }
    // StackView 中从子页面返回时 onVisibleChanged 未必触发，
    // 用 onActivated 保证每次回到本页都刷新
    StackView.onActivated: {
        authViewModel.loadProfile()
        notifyVM.loadNotifications(1, 20)
    }

    // 居中容器，限制最大宽度，防止按钮随窗口放大
    Item {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge

        ColumnLayout {
            width: Math.min(parent.width, 400)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            spacing: Theme.spacingMedium

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingSmall

            // ★ 圆形头像（ShaderEffect 遮罩，全 Qt 版本兼容）
            CircularImage {
                id: profileAvatar
                Layout.alignment: Qt.AlignHCenter
                width: 72; height: 72
                borderColor: Theme.dividerColor
                borderWidth: 1.5
                placeholderFallback: {
                    var n = authViewModel.profileDisplayName
                    if (n.length > 0) return n.charAt(0).toUpperCase()
                    n = authViewModel.username
                    if (n.length > 0) return n.charAt(0).toUpperCase()
                    return "G"
                }
                placeholderText.font.family: Theme.fontFamily
                placeholderText.font.weight: Theme.fontWeightMedium
                placeholderText.color: Theme.textHint

                // 监听 profileAvatarUrl 变化
                Connections {
                    target: authViewModel
                    function onProfileChanged() {
                        var url = authViewModel.profileAvatarUrl
                        if (url.length > 0)
                            profileAvatar.source = authViewModel.apiBaseUrl + url
                        else
                            profileAvatar.source = ""
                    }
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
            buttonText: qsTr("账号与安全")
            buttonType: CustomButton.ButtonType.Secondary
            visible: authViewModel.loggedIn
            onClicked: showAccountSecurityRequest()
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

    // ========== 消息通知图标（右上角） ==========
    Item {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: -Theme.spacingSmall
        anchors.rightMargin: -Theme.spacingSmall
        width: 44; height: 44
        visible: authViewModel.loggedIn

        Button {
            anchors.fill: parent
            flat: true
            contentItem: Text {
                text: "\uD83D\uDCE2"
                font.pointSize: 22
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: showNotificationRequest()
        }

        // 未读红点
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 2
            anchors.rightMargin: 0
            width: 12; height: 12
            radius: 6
            color: "#E74C3C"
            visible: notifyVM.unreadCount > 0

            Text {
                anchors.centerIn: parent
                text: notifyVM.unreadCount > 99 ? "99+" : notifyVM.unreadCount.toString()
                color: "white"
                font.pointSize: 8
                font.weight: Font.Bold
                visible: notifyVM.unreadCount > 0
            }
        }
    }
}
}
