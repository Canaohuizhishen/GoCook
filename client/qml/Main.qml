import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import client
import "./components"
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

    // 全局网络失败提示（底部黑色 toast；网络层错误统一中文文案，服务端业务错误显示具体信息）
    Connections {
        target: httpApi
        function onNetworkError(message) {
            // 先清空再赋值：同文案连续错误也能重启自动消失计时
            networkErrorBanner.text = ""
            networkErrorBanner.text = message
        }
    }

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: isLoading ? loadingComponent :
                     isLoggedIn ? homePage : loginPage

        // 页面推入动画（淡入）
        // 注：x 动画已移除，因为 anchors.fill:parent 与直接赋值 x 冲突
        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 240
                easing.type: Easing.OutCubic
            }
        }

        // 页面推出动画（淡出）
        pushExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1.0
                to: 0.3
                duration: 200
                easing.type: Easing.InCubic
            }
        }

        // 页面返回动画（淡入）
        popEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0.3
                to: 1.0
                duration: 240
                easing.type: Easing.OutCubic
            }
        }

        // 页面返回动画（淡出）
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 200
                easing.type: Easing.InCubic
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

    // 全局网络失败提示（底部黑色 toast，1.5 秒自动消失；PDD/微信等主流 App 形态，替代顶部红条）
    ErrorBanner {
        id: networkErrorBanner
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 100
        anchors.horizontalCenter: parent.horizontalCenter
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
                coverImageDialog.openWithFilter(qsTr("选择封面图片"), "图片文件 (*.jpg *.jpeg *.png *.gif *.bmp *.svg)")
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
            onShowRecommendFromInventory: () => {
                stackView.push(recommendResultsPage)
            }
            onShowSettingsRequest: () => {
                stackView.push(settingsPage)
            }
            onShowNotificationRequest: () => {
                stackView.push(notificationPage)
            }
            onShowShoppingListRequest: () => {
                stackView.push(shoppingListPage)
            }
        }
    }

    Component {
        id: recipeDetailPage
        RecipeDetailPage {
            property var _stackView: stackView
        }
    }

    NativeFileDialog {
        id: coverImageDialog
        onFileSelected: function(path) {
            stackView.push(submitRecipePage, { coverImagePath: path, _stackView: stackView })
        }
    }

    Component {
        id: submitRecipePage
        SubmitRecipePage {
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
        id: recommendResultsPage
        RecommendResultsPage {
            property var _stackView: stackView
            onRecipeClicked: (recipeId) => {
                stackView.push(recipeDetailPage, {recipeId: recipeId})
            }
        }
    }

    Component {
        id: shoppingListPage
        ShoppingListPage {
            onGoBack: {
                stackView.pop()
            }
            onShowDetailRequest: (listId) => {
                stackView.push(shoppingListDetailPage, {listId: listId})
            }
        }
    }

    Component {
        id: profileEditPage
        ProfileEditPage { }
    }

    Component {
        id: settingsPage
        SettingsPage {
            onEditProfileRequest: () => {
                stackView.push(profileEditPage)
            }
            onAccountSecurityRequest: () => {
                stackView.push(accountSecurityPage)
            }
            onDietaryPreferencesRequest: () => {
                stackView.push(preferencesPage)
            }
            onHealthProfileRequest: () => {
                stackView.push(healthProfilePage)
            }
        }
    }

    Component {
        id: shoppingListDetailPage
        ShoppingListDetailPage {
            onGoBack: {
                stackView.pop()
            }
        }
    }

    Component {
        id: preferencesPage
        PreferencesPage { }
    }

    Component {
        id: healthProfilePage
        HealthProfilePage { }
    }

    Component {
        id: changePasswordPage
        ChangePasswordPage { }
    }

    Component {
        id: accountSecurityPage
        AccountSecurityPage {
            onShowChangePasswordRequest: () => {
                stackView.push(changePasswordPage)
            }
        }
    }

    Component {
        id: notificationPage
        NotificationPage {
            property var _stackView: stackView
            onShowDetailRequest: (data) => {
                stackView.push(notificationDetailPage, {notificationData: data})
            }
        }
    }

    Component {
        id: notificationDetailPage
        NotificationDetailPage {
            property var _stackView: stackView
        }
    }

    Component {
        id: loadingComponent
        Item {
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
