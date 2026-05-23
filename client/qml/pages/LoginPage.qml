import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: loginPage
    title: qsTr("登录")

    property bool showResetFlow: false
    property bool showResetForm: false

    function resetToLogin() {
        showResetFlow = false
        showResetForm = false
        resetUsernameField.text = ""
        resetEmailField.text = ""
        resetTokenField.text = ""
        resetNewPasswordField.text = ""
        errorLabel.text = ""
    }

    // 居中布局
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width * 0.8
        spacing: 20

        Text {
            text: "GoCook"
            font.pointSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // ============ 登录/注册模式 ============
        ColumnLayout {
            visible: !showResetFlow
            spacing: 20
            Layout.fillWidth: true

            TextField {
                id: usernameField
                placeholderText: qsTr("用户名")
                Layout.fillWidth: true
            }

            TextField {
                id: passwordField
                placeholderText: qsTr("密码")
                echoMode: TextInput.Password
                Layout.fillWidth: true
            }

            TextField {
                id: emailField
                visible: false
                placeholderText: qsTr("邮箱")
                Layout.fillWidth: true
            }

            CustomButton {
                buttonText: qsTr("登录")
                buttonType: CustomButton.ButtonType.Primary
                Layout.fillWidth: true
                onClicked: {
                    authViewModel.login(usernameField.text, passwordField.text)
                }
            }

            CustomButton {
                buttonText: qsTr("注册")
                buttonType: CustomButton.ButtonType.Secondary
                Layout.fillWidth: true
                onClicked: {
                    if(emailField.visible === false)emailField.visible = true
                    else {
                        authViewModel.registerUser(
                            usernameField.text,
                            passwordField.text,
                            emailField.text
                        )
                    }
                }
            }

            // 忘记密码链接
            Text {
                text: qsTr("忘记密码？")
                color: Theme.accentColor
                font.underline: true
                Layout.alignment: Qt.AlignHCenter
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        showResetFlow = true
                        errorLabel.text = ""
                    }
                }
            }
        }

        // ============ 忘记密码/重置密码模式 ============
        ColumnLayout {
            visible: showResetFlow
            spacing: 20
            Layout.fillWidth: true

            Text {
                text: showResetForm ? qsTr("重置密码") : qsTr("忘记密码")
                font.pointSize: 16
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            // 第一步：输入用户名+邮箱发送重置邮件
            ColumnLayout {
                visible: !showResetForm
                spacing: 15
                Layout.fillWidth: true

                Text {
                    text: qsTr("请输入您的用户名和注册邮箱，我们将发送重置链接。")
                    color: Theme.textHint
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                TextField {
                    id: resetUsernameField
                    placeholderText: qsTr("用户名")
                    Layout.fillWidth: true
                }

                TextField {
                    id: resetEmailField
                    placeholderText: qsTr("邮箱")
                    Layout.fillWidth: true
                }

                CustomButton {
                    buttonText: qsTr("发送重置邮件")
                    buttonType: CustomButton.ButtonType.Primary
                    Layout.fillWidth: true
                    onClicked: {
                        if (resetUsernameField.text.trim() === "") {
                            errorLabel.text = "请输入用户名"
                            return
                        }
                        if (resetEmailField.text.trim() === "") {
                            errorLabel.text = "请输入邮箱地址"
                            return
                        }
                        authViewModel.forgotPassword(resetUsernameField.text, resetEmailField.text)
                    }
                }
            }

            // 第二步：输入 token + 新密码
            ColumnLayout {
                visible: showResetForm
                spacing: 15
                Layout.fillWidth: true

                Text {
                    text: qsTr("请输入邮件中的重置令牌和新密码。")
                    color: Theme.textHint
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                TextField {
                    id: resetTokenField
                    placeholderText: qsTr("重置令牌")
                    Layout.fillWidth: true
                }

                TextField {
                    id: resetNewPasswordField
                    placeholderText: qsTr("新密码（至少6位，含字母和数字）")
                    echoMode: TextInput.Password
                    Layout.fillWidth: true
                }

                CustomButton {
                    buttonText: qsTr("重置密码")
                    buttonType: CustomButton.ButtonType.Primary
                    Layout.fillWidth: true
                    onClicked: {
                        if (resetTokenField.text.trim() === "") {
                            errorLabel.text = "请输入重置令牌"
                            return
                        }
                        if (resetNewPasswordField.text.length < 6) {
                            errorLabel.text = "密码不能少于6个字符"
                            return
                        }
                        authViewModel.resetPassword(resetTokenField.text, resetNewPasswordField.text)
                    }
                }
            }

            // 返回登录
            Text {
                text: qsTr("← 返回登录")
                color: Theme.accentColor
                font.underline: true
                Layout.alignment: Qt.AlignHCenter
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: resetToLogin()
                }
            }
        }

        // ============ 提示标签 ============
        Label {
            id: errorLabel
            color: Theme.errorColor
            visible: text !== ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        // ============ 信号连接 ============
        Connections {
            target: authViewModel
            function onLoginFailed(error) {
                errorLabel.color = Theme.errorColor
                errorLabel.text = error
            }
            function onRegisterFailed(error) {
                errorLabel.color = Theme.errorColor
                errorLabel.text = error
            }
            function onRegisterSuccess() {
                errorLabel.color = Theme.accentColor
                errorLabel.text = "注册成功，请登录"
            }
            function onLoginSuccess() {
                errorLabel.text = ""
            }
            function onForgotPasswordSent() {
                errorLabel.color = Theme.accentColor
                errorLabel.text = "若该邮箱已注册，您将收到重置邮件。请在服务端日志中查看重置令牌。"
                showResetForm = true
            }
            function onForgotPasswordFailed(error) {
                errorLabel.color = Theme.errorColor
                errorLabel.text = error
            }
            function onPasswordResetSuccess() {
                errorLabel.color = Theme.accentColor
                errorLabel.text = "密码重置成功，请登录"
                resetToLogin()
            }
            function onPasswordResetFailed(error) {
                errorLabel.color = Theme.errorColor
                errorLabel.text = error
            }
        }

    }
}