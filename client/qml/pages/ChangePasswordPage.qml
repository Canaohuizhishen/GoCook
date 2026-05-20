import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client

Page {
    id: changePwdPage
    title: qsTr("修改密码")

    function goBack() {
        var item = changePwdPage.parent
        while (item) {
            try { if (typeof item.pop === "function") { item.pop(); return } } catch(e) {}
            item = item.parent
        }
        var sv = StackView.view
        if (sv && typeof sv.pop === "function") sv.pop()
    }

    // ========== 页面内容 ==========
    Item {
        anchors.fill: parent
        ScrollView {
            anchors.fill: parent; clip: true; contentWidth: availableWidth
            Column {
                width: Math.min(parent.width - Theme.spacingLarge * 2, 400)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top; anchors.topMargin: Theme.spacingXLarge
                spacing: Theme.spacingMedium

                // ========== 标题 ==========
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("修改密码")
                    font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeH2
                    font.weight: Theme.fontWeightBold; color: Theme.textPrimary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("请先验证当前密码，然后设置新密码")
                    font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                    color: Theme.textHint
                    wrapMode: Text.WordWrap
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.dividerColor }

                // ========== 当前密码 ==========
                ColumnLayout { width: parent.width; spacing: Theme.spacingXSmall
                    Text { text: qsTr("当前密码"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                    Rectangle {
                        Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                        color: Theme.cardBackground; border.color: Theme.dividerColor
                        TextInput {
                            id: currentPwdInput
                            anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                            verticalAlignment: TextInput.AlignVCenter
                            echoMode: TextInput.Password
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                            clip: true; selectByMouse: true
                        }
                    }
                }

                // ========== 新密码 ==========
                ColumnLayout { width: parent.width; spacing: Theme.spacingXSmall
                    Text { text: qsTr("新密码"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                    Rectangle {
                        Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                        color: Theme.cardBackground; border.color: Theme.dividerColor
                        TextInput {
                            id: newPwdInput
                            anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                            verticalAlignment: TextInput.AlignVCenter
                            echoMode: TextInput.Password
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                            clip: true; selectByMouse: true
                        }
                    }
                }

                // ========== 确认新密码 ==========
                ColumnLayout { width: parent.width; spacing: Theme.spacingXSmall
                    Text { text: qsTr("确认新密码"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                    Rectangle {
                        Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                        color: Theme.cardBackground; border.color: Theme.dividerColor
                        TextInput {
                            id: confirmPwdInput
                            anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                            verticalAlignment: TextInput.AlignVCenter
                            echoMode: TextInput.Password
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                            clip: true; selectByMouse: true
                        }
                    }
                }

                // ========== 状态提示 ==========
                Text {
                    id: statusText; width: parent.width; height: 20
                    font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                    color: Theme.accentColor; horizontalAlignment: Text.AlignHCenter
                    visible: text.length > 0
                }

                // ========== 确认修改按钮 ==========
                Button {
                    id: confirmBtn; width: parent.width; height: 50
                    text: qsTr("确认修改")
                    background: Rectangle { radius: Theme.radiusMedium; color: Theme.primaryColor }
                    contentItem: Text {
                        text: confirmBtn.text; font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium
                        color: Theme.textOnPrimary; horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        var old = currentPwdInput.text
                        var newPwd = newPwdInput.text
                        var confirm = confirmPwdInput.text
                        if (old.length === 0 || newPwd.length === 0) {
                            statusText.text = qsTr("请填写完整")
                            return
                        }
                        if (newPwd !== confirm) {
                            statusText.text = qsTr("两次密码输入不一致")
                            return
                        }
                        statusText.text = qsTr("正在修改...")
                        authViewModel.changePassword(old, newPwd)
                    }
                }

                // ========== 返回按钮 ==========
                Button {
                    id: backBtn; width: parent.width; height: 50; text: qsTr("返回")
                    background: Rectangle {
                        radius: Theme.radiusMedium; color: "transparent"
                        border.color: Theme.primaryColor; border.width: 1
                    }
                    contentItem: Text {
                        text: backBtn.text; font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody; color: Theme.primaryColor
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: changePwdPage.goBack()
                }

                Item { width: 1; height: Theme.spacingXLarge }
            }
        }
    }

    Connections {
        target: authViewModel
        function onPasswordChanged() {
            console.log("[QML] passwordChanged")
            statusText.text = qsTr("密码修改成功")
            currentPwdInput.text = ""
            newPwdInput.text = ""
            confirmPwdInput.text = ""
            // 成功后稍等片刻自动返回
            changePwdPage.goBack()
        }
        function onPasswordChangeFailed(error) {
            console.log("[QML] passwordChangeFailed: " + error)
            statusText.text = qsTr("密码修改失败: ") + error
        }
    }
}
