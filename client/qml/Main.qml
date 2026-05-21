import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.settings
import "./pages"

ApplicationWindow {
    id: appWindow
    visible: true
    width: 400
    height: 700
    minimumWidth: 320
    minimumHeight: 480
    title: qsTr("GoCook")

    property bool isLoggedIn: authViewModel.loggedIn
    property bool isLoading: authViewModel.initialLoading

    Settings {
        id: settings
        property int themeMode: 0
    }

    Component.onCompleted: Theme.themeMode = settings.themeMode

    Connections {
        target: Theme
        function onThemeModeChanged() {
            settings.themeMode = Theme.themeMode
        }
    }

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: isLoading ? loadingComponent :
                     isLoggedIn ? homePage : loginPage

        // 页面推入动画（新页从右侧滑入）
        pushEnter: Transition {
            ParallelAnimation {
                PropertyAnimation {
                    property: "x"
                    from: stackView.width * 0.3
                    to: 0
                    duration: 280
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                    duration: 240
                    easing.type: Easing.OutCubic
                }
            }
        }

        // 页面推出动画（旧页向左淡出）
        pushExit: Transition {
            ParallelAnimation {
                PropertyAnimation {
                    property: "x"
                    from: 0
                    to: -stackView.width * 0.2
                    duration: 280
                    easing.type: Easing.InCubic
                }
                PropertyAnimation {
                    property: "opacity"
                    from: 1.0
                    to: 0.3
                    duration: 200
                    easing.type: Easing.InCubic
                }
            }
        }

        // 页面返回动画（当前页向右滑出）
        popEnter: Transition {
            ParallelAnimation {
                PropertyAnimation {
                    property: "x"
                    from: -stackView.width * 0.2
                    to: 0
                    duration: 280
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    property: "opacity"
                    from: 0.3
                    to: 1.0
                    duration: 240
                    easing.type: Easing.OutCubic
                }
            }
        }

        // 页面返回动画（新页从左侧出现）
        popExit: Transition {
            ParallelAnimation {
                PropertyAnimation {
                    property: "x"
                    from: 0
                    to: stackView.width * 0.3
                    duration: 280
                    easing.type: Easing.InCubic
                }
                PropertyAnimation {
                    property: "opacity"
                    from: 1.0
                    to: 0.0
                    duration: 200
                    easing.type: Easing.InCubic
                }
            }
        }

        // replace 过渡（登录/登出切换时用）
        replaceEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
        replaceExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 150
            }
        }
    }

    Component {
        id: loginPage
        LoginPage { }
    }

    Component {
        id: homePage
        HomePage {
            onShowDetailRequest: (recipeId) => {
                stackView.push(recipeDetailPage, {recipeId: recipeId})
            }
            onShowSubmitRequest: () => {
                stackView.push(submitRecipePage)
            }
            onShowMyRecipesRequest: () => {
                stackView.push(myRecipesPage)
            }
            onShowMyRatingsRequest: () => {
                stackView.push(myRatingsPage)
            }
            onShowSearchRequest: () => {
                stackView.push(searchPage)
            }
        }
    }

    Component {
        id: recipeDetailPage
        RecipeDetailPage {
            property var _stackView: stackView
        }
    }

    Component {
        id: submitRecipePage
        SubmitRecipePage {
            property var _stackView: stackView
        }
    }

    Component {
        id: myRecipesPage
        MyRecipesPage {
            property var _stackView: stackView
        }
    }

    Component {
        id: myRatingsPage
        MyRatingsPage {
            property var _stackView: stackView
        }
    }

    Component {
        id: searchPage
        SearchPage {
            property var _stackView: stackView
            onRecipeClicked: (recipeId) => {
                stackView.push(recipeDetailPage, {recipeId: recipeId})
            }
        }
    }

    Component {
        id: loadingComponent
        Item {
            anchors.fill: parent
            Text {
                anchors.centerIn: parent
                text: qsTr("加载中...")
                font.pointSize: 15
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
