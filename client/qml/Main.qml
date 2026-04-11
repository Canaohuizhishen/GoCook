import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./pages"

ApplicationWindow {
    id: appWindow
    visible: true
    width: 400
    height: 700
    title: qsTr("GoCook")

    // 全局状态：从 C++ AuthManager 读取登录状态
    property bool isLoggedIn: authManager.loggedIn

    // 主路由栈
    StackView {
        id: stackView
        anchors.fill: parent
        // 根据登录状态决定初始页面
        initialItem: isLoggedIn ? homePage : loginPage
    }

    // 登录页面组件
    Component {
        id: loginPage
        LoginPage { }
    }

    // 主页组件（包含底部导航）
    Component {
        id: homePage
        HomePage { }
    }

    // 监听登录状态变化，自动切换页面
    Connections {
        target: authManager
        function onLoggedInChanged() {
            if (authManager.loggedIn) {
                // 登录成功，切换到主页
                stackView.replace(homePage)
            } else {
                // 登出，返回登录页
                stackView.replace(loginPage)
            }
        }
    }
}