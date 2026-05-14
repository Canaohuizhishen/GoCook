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

    // 全局状态：从 C++ AuthViewModel 读取登录状态
    property bool isLoggedIn: authViewModel.loggedIn
    property bool isLoading: authViewModel.initialLoading

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: isLoading ? loadingComponent :
                     isLoggedIn ? homePage : loginPage
    }

    Component {
        id: loginPage
        LoginPage { }
    }

    Component {
        id: homePage
        HomePage { }
    }

    Component {
        id: loadingComponent
        // 应用启动时的过渡页，在自动登录检查完成前避免闪现登录页
        Item {
            anchors.fill: parent
            Text {
                anchors.centerIn: parent
                text: qsTr("加载中...")
                font.pixelSize: 20
            }
        }
    }

    Connections {
        target: authViewModel
        function onInitialLoadingChanged() {
            if (!authViewModel.initialLoading) {
                if (authViewModel.loggedIn) {
                    stackView.replace(homePage)
                } else {
                    stackView.replace(loginPage)
                }
            }
        }
        function onLoggedInChanged() {
            if (authViewModel.initialLoading) return
            if (authViewModel.loggedIn) {
                stackView.replace(homePage)
            } else {
                stackView.replace(loginPage)
            }
        }
    }
}