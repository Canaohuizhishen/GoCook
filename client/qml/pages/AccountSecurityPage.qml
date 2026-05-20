import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: accountSecurityPage
    title: qsTr("账号与安全")

    signal showChangePasswordRequest()

    function goBack() {
        var sv = StackView.view
        if (sv && typeof sv.pop === "function") sv.pop()
    }

    // ========== 页面内容 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingMedium

        // ========== 标题区 ==========
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingXSmall

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("账号与安全")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH2
                font.weight: Theme.fontWeightBold
                color: Theme.textPrimary
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("管理账号安全设置")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textHint
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.dividerColor
            Layout.topMargin: Theme.spacingSmall
        }

        // ========== 修改密码 ==========
        CustomButton {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingMedium
            buttonText: qsTr("修改密码")
            buttonType: CustomButton.ButtonType.Secondary
            onClicked: showChangePasswordRequest()
        }

        // ========== 注销账户 ==========
        CustomButton {
            Layout.fillWidth: true
            buttonText: qsTr("注销账户")
            buttonType: CustomButton.ButtonType.Secondary
            onClicked: deleteAccountDialog.open()
        }

        Item { Layout.fillHeight: true }

        // ========== 返回按钮 ==========
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            text: qsTr("返回")
            background: Rectangle {
                radius: Theme.radiusMedium
                color: "transparent"
                border.color: Theme.primaryColor
                border.width: 1
            }
            contentItem: Text {
                text: parent.text
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: Theme.primaryColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: accountSecurityPage.goBack()
        }
    }

    // ========== 注销账户确认对话框 ==========
    Dialog {
        id: deleteAccountDialog
        modal: true
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: Math.min(parent.width * 0.85, 360)
        height: dialogColumn.implicitHeight + Theme.spacingXLarge * 2

        background: Rectangle {
            radius: Theme.radiusLarge
            color: Theme.cardBackground
        }

        ColumnLayout {
            id: dialogColumn
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingMedium

            Text {
                Layout.fillWidth: true
                text: qsTr("确认注销账户")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH2
                font.weight: Theme.fontWeightBold
                color: Theme.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("注销后，您的个人资料、偏好和库存数据将被永久删除。\n已发布的菜谱和评论将保持匿名保留。\n\n此操作不可撤销，确定要继续吗？")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textHint
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.4
            }

            Text {
                id: deleteStatusText
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.accentColor
                horizontalAlignment: Text.AlignHCenter
                visible: text.length > 0
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    text: qsTr("取消")
                    background: Rectangle {
                        radius: Theme.radiusMedium
                        color: "transparent"
                        border.color: Theme.dividerColor
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        font.family: Theme.fontFamily
                        color: Theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        deleteAccountDialog.close()
                        deleteStatusText.text = ""
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    text: qsTr("确认注销")
                    background: Rectangle {
                        radius: Theme.radiusMedium
                        color: "#e74c3c"
                    }
                    contentItem: Text {
                        text: parent.text
                        font.family: Theme.fontFamily
                        font.weight: Theme.fontWeightMedium
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        deleteStatusText.text = qsTr("正在注销...")
                        authViewModel.deleteAccount()
                    }
                }
            }
        }
    }

    // ========== 注销账户结果处理 ==========
    Connections {
        target: authViewModel
        function onAccountDeleted() {
            deleteAccountDialog.close()
            deleteStatusText.text = ""
            // 注销后自动登出，回到登录页
            accountSecurityPage.goBack()
        }
        function onAccountDeleteFailed(error) {
            deleteStatusText.text = qsTr("注销失败: ") + error
        }
    }
}
