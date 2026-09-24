import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: loginPage
    title: qsTr("登录")

    // 欢迎模式（冷启动首次）：标题显示欢迎语、右上角「先逛逛」，点击即进入游客模式
    property bool welcomeMode: false
    // 应用内模式（需要登录的功能触发）：右上角「取消」，点击返回原页面（取消操作）
    property bool inAppMode: false
    signal skipRequested()
    signal closeRequested()

    property bool showResetFlow: false
    property bool showResetForm: false
    property bool showRegisterVerify: false   // 两段式注册：验证码步骤
    // 登录/注册显式模式：注册态 = 多一行邮箱 + 主按钮改为注册；两态用页内链接互切
    // （照“忘记密码”链接范式）——不再用 emailField.visible 暗传模式（曾致“点注册后回不了登录”）
    property bool registerMode: false

    function resetToLogin() {
        showResetFlow = false
        showResetForm = false
        showRegisterVerify = false
        registerMode = false   // 任何路径都回到登录态（注册成功 / 验证码返回 / 重置密码完成）
        resetUsernameField.text = ""
        resetEmailField.text = ""
        resetTokenField.text = ""
        resetNewPasswordField.text = ""
        verifyCodeField.text = ""
        emailField.text = ""   // email 属注册域字段：随注册域退场（username/password 保留供直接登录）
        errorLabel.text = ""
    }

    // 键盘提交入口（Enter）：密码框 Enter 直接登录（注册态先跳邮箱），邮箱 Enter 直接注册
    function submitLogin() {
        authViewModel.login(usernameField.text, passwordField.text)
    }
    function submitRegister() {
        authViewModel.registerUser(
            usernameField.text,
            passwordField.text,
            emailField.text
        )
    }

    // 两段式注册第二步：提交验证码
    function submitVerify() {
        if (verifyCodeField.text.trim() === "") {
            errorLabel.text = "请输入验证码"
            return
        }
        authViewModel.verifyRegistration(emailField.text.trim(), verifyCodeField.text.trim())
    }

    // 顶部品牌渐变带（欢迎/应用内两态共用——避免两态观感割裂；低饱和品牌色 → 透明，纯装饰不拦事件）
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 300
        visible: loginPage.welcomeMode || loginPage.inAppMode
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b,
                               Theme.isDarkMode ? 0.10 : 0.16)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.0)
            }
        }
    }

    // 顶部工具行：按钮固定页面右上角，文案随模式（欢迎=先逛逛 / 应用内=取消）
    RowLayout {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.spacingSmall
        anchors.rightMargin: Theme.spacingSmall
        visible: loginPage.welcomeMode || loginPage.inAppMode
        Button {
            flat: true
            text: loginPage.welcomeMode ? qsTr("先逛逛") : qsTr("取消")
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

        // ============ 品牌 Hero 区（两态共用：欢迎态 = 欢迎语 + 价值主张；应用内态 = 橙色字标 + 来意说明） ============
        ColumnLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 12
            spacing: 8

            // 标题（字标）：品牌词 GoCook 用主色；欢迎态带欢迎语并放大到 30pt，应用内态为橙色字标 26pt
            // （无图形徽章——方案 D 定稿；颜色随主题，修暗色模式黑字不可读）
            // 居中写法：嵌套 ColumnLayout 下仅靠 Layout.alignment 不会撑满父列（实测列宽收缩→标题左偏），
            // 必须由子项 fillWidth 把列撑满 + horizontalAlignment 文本自居中（2026-09-24 实测结论）
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.StyledText
                text: loginPage.welcomeMode
                      ? qsTr("欢迎使用 <font color=\"%1\">GoCook</font>").arg(Theme.primaryColor.toString())
                      : "GoCook"
                color: loginPage.welcomeMode ? Theme.textPrimary : Theme.primaryColor
                font.family: Theme.fontFamily
                font.pointSize: loginPage.welcomeMode ? 30 : 26
                font.bold: true
            }

            // 副标题（登录/注册 × 欢迎/应用内 四态）：登录态=同步资产/交代来意；
            // 注册态=邀请加入/继续来意（不能沿用"同步"：未注册用户尚无数据可同步）
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                // 折行防护：注册态文案较长，窄窗下折行而不是横向溢出（原 NoWrap 会溢出）
                wrapMode: Text.Wrap
                visible: loginPage.welcomeMode || loginPage.inAppMode
                text: loginPage.registerMode
                      ? (loginPage.welcomeMode ? qsTr("加入 GoCook，开始整理你的菜谱、收藏与库存")
                                               : qsTr("注册后继续你刚才的操作"))
                      : (loginPage.welcomeMode ? qsTr("登录后同步你的菜谱、收藏与库存")
                                               : qsTr("登录后继续你刚才的操作"))
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
            }
        }

        // ============ 登录/注册模式（registerMode 显式二态；链接互切） ============
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
                    if (loginPage.registerMode)
                        emailField.forceActiveFocus()
                    else
                        submitLogin()
                }
                Keys.onEnterPressed: {
                    if (loginPage.registerMode)
                        emailField.forceActiveFocus()
                    else
                        submitLogin()
                }
            }

            TextField {
                id: emailField
                visible: loginPage.registerMode   // 可见性仅由显式模式驱动（无手动写入点）
                placeholderText: qsTr("邮箱")
                Layout.fillWidth: true
                Keys.onReturnPressed: submitRegister()
                Keys.onEnterPressed: submitRegister()
            }

            CustomButton {
                buttonText: loginPage.registerMode ? qsTr("注册") : qsTr("登录")
                buttonType: CustomButton.ButtonType.Primary
                Layout.fillWidth: true
                onClicked: loginPage.registerMode ? submitRegister() : submitLogin()
            }

            // 登录/注册互切 + 忘记密码（链接组，间距收紧）
            ColumnLayout {
                spacing: 12
                Layout.alignment: Qt.AlignHCenter

                Text {
                    text: loginPage.registerMode ? qsTr("已有账号？去登录") : qsTr("没有账号？去注册")
                    color: Theme.accentColor
                    font.underline: true
                    Layout.alignment: Qt.AlignHCenter
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            loginPage.registerMode = !loginPage.registerMode
                            if (!loginPage.registerMode)
                                emailField.text = ""   // 离开注册域：注册专属字段随之清空
                            errorLabel.text = ""
                        }
                    }
                }

                // 忘记密码链接（仅登录态显示）
                Text {
                    visible: !loginPage.registerMode
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
                // 先复位再设文案：resetToLogin 会清空 errorLabel，顺序倒置将吞掉成功提示
                resetToLogin()
                errorLabel.color = Theme.accentColor
                errorLabel.text = "密码重置成功，请登录"
            }
            function onPasswordResetFailed(error) {
                errorLabel.color = Theme.errorColor
                errorLabel.text = error
            }
        }

    }
}