import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    id: loginPage
    title: qsTr("登录")

    // 居中布局
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width * 0.8
        spacing: 20

        Text {
            text: "GoCook"
            font.pixelSize: 32
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

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

        Label {
            id: errorLabel
            color: "red"
            visible: text !== ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        // 连接认证管理器的信号
        Connections {
            target: authViewModel
            function onLoginFailed(error) {
                errorLabel.color = "red"
                errorLabel.text = error
            }
            function onRegisterFailed(error) {
                errorLabel.color = "red"
                errorLabel.text = error
            }
            function onRegisterSuccess() {
                errorLabel.color = "green"  // 成功消息用绿色与错误红色区分
                errorLabel.text = "注册成功，请登录"
            }
            function onLoginSuccess() {
                errorLabel.text = ""
            }
        }
    }
}