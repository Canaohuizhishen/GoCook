import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: loginPage
    title: qsTr("登录")

    // 欢迎模式（冷启动首次）：显示「跳过，先逛逛」，跳过即进入游客模式
    property bool welcomeMode: false
    // 应用内模式（需要登录的功能触发）：显示关闭按钮，可返回原页面（取消操作）
    property bool inAppMode: false
    signal skipRequested()
    signal closeRequested()

    property bool showResetFlow: false
    property bool showResetForm: false
    property bool showRegisterVerify: false   // 两段式注册：验证码步骤

    function resetToLogin() {
        showResetFlow = false
        showResetForm = false
        showRegisterVerify = false
        resetUsernameField.text = ""
        resetEmailField.text = ""
        resetTokenField.text = ""
        resetNewPasswordField.text = ""
        verifyCodeField.text = ""
        errorLabel.text = ""
    }

    // 键盘提交入口（Enter）：密码框 Enter 直接登录，邮箱 Enter 直接注册
    function submitLogin() {
        authViewModel.login(usernameField.text, passwordField.text)
    }
    function registerAction() {
        if (emailField.visible === false) emailField.visible = true
        else {
            authViewModel.registerUser(
                usernameField.text,
                passwordField.text,
                emailField.text
            )
        }
    }

    // 两段式注册第二步：提交验证码
    function submitVerify() {
        if (verifyCodeField.text.trim() === "") {
            errorLabel.text = "请输入验证码"
            return
        }
        authViewModel.verifyRegistration(emailField.text.trim(), verifyCodeField.text.trim())
    }

    // 顶部工具行：跳过按钮固定页面右上角（欢迎模式=进入游客模式，应用内模式=取消操作）
    RowLayout {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.spacingSmall
        anchors.rightMargin: Theme.spacingSmall
        visible: loginPage.welcomeMode || loginPage.inAppMode
        Button {
            flat: true
            text: qsTr("跳过")
            font.pointSize: Theme.fontSizeBody
            onClicked: {
                if (loginPage.welcomeMode)
                    loginPage.skipRequested()
                else
                    loginPage.closeRequested()
            }
        }
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
            visible: !showResetFlow && !showRegisterVerify
            spacing: 20
            Layout.fillWidth: true

            TextField {
                id: usernameField
                placeholderText: qsTr("用户名")
                Layout.fillWidth: true
                Keys.onReturnPressed: passwordField.forceActiveFocus()
                Keys.onEnterPressed: passwordField.forceActiveFocus()
            }

            TextField {
                id: passwordField
                placeholderText: qsTr("密码")
                echoMode: TextInput.Password
                Layout.fillWidth: true
                Keys.onReturnPressed: {
                    if (emailField.visible)
                        emailField.forceActiveFocus()
                    else
                        submitLogin()
                }
                Keys.onEnterPressed: {
                    if (emailField.visible)
                        emailField.forceActiveFocus()
                    else
                        submitLogin()
                }
            }

            TextField {
                id: emailField
                visible: false
                placeholderText: qsTr("邮箱")
                Layout.fillWidth: true
                Keys.onReturnPressed: registerAction()
                Keys.onEnterPressed: registerAction()
            }

            CustomButton {
                buttonText: qsTr("登录")
                buttonType: CustomButton.ButtonType.Primary
                Layout.fillWidth: true
                onClicked: submitLogin()
            }

            CustomButton {
                buttonText: qsTr("注册")
                buttonType: CustomButton.ButtonType.Secondary
                Layout.fillWidth: true
                onClicked: registerAction()
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

        // ============ 注册验证码模式（两段式注册第二步） ============
        ColumnLayout {
            visible: showRegisterVerify
            spacing: 20
            Layout.fillWidth: true

            Text {
                text: qsTr("完成注册")
                font.pointSize: 16
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: qsTr("我们已向该邮箱发送邮件，请按邮件提示继续。")
                color: Theme.textHint
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            TextField {
                id: verifyCodeField
                placeholderText: qsTr("验证码（6 位数字）")
                Layout.fillWidth: true
                Keys.onReturnPressed: submitVerify()
                Keys.onEnterPressed: submitVerify()
            }

            CustomButton {
                buttonText: qsTr("完成注册")
                buttonType: CustomButton.ButtonType.Primary
                Layout.fillWidth: true
                onClicked: submitVerify()
            }

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
            function onRegisterStarted() {
                errorLabel.text = ""
                showRegisterVerify = true
            }
            function onRegistrationVerified() {
                resetToLogin()
                errorLabel.color = Theme.accentColor
                errorLabel.text = "注册成功，请登录"
            }
            function onRegistrationVerifyFailed(error) {
                errorLabel.color = Theme.errorColor
                errorLabel.text = error
            }
            function onLoginSuccess() {
                errorLabel.text = ""
            }
            function onForgotPasswordSent() {
                errorLabel.color = Theme.accentColor
                errorLabel.text = "重置密码邮件已发送，请检查收件箱和垃圾邮件。（开发模式下验证码见服务端日志）"
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