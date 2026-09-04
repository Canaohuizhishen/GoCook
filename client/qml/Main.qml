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

    // 被导航/动作守卫拦下的挂起意图（登录成功后消费）：
    //   kind="push"   → 继续之前被拦下的页面导航（replace 目标页）
    //   kind="action" → 执行被拦下的动作回调（pop 回原页面后执行）
    // 未登录连点多个需登录操作时，最新意图覆盖旧意图（防重入见 loginPageOpen）
    property var pendingNav: null
    // 应用内登录页是否已在栈顶（guardedPush/guardAction/onAuthRequired 共用防重入：登录页只压一层）
    property bool loginPageOpen: false

    // 统一导航守卫：所有页面 push 的唯一入口。
    // requiresLogin=true 且未登录时，先弹应用内登录页（inAppMode），登录成功后继续原导航；
    // 登录页被关闭则取消原导航。需登录页面在调用点显式传 true（页面根对象上的 requiresLogin 声明
    // 不会被 Component 对象暴露，无法作为判定依据）。
    function guardedPush(component, props, requiresLogin) {
        if (requiresLogin && !authViewModel.loggedIn) {
            pendingNav = {kind: "push", component: component, props: props === undefined ? {} : props}
            openInAppLogin()
        } else {
            stackView.push(component, props)
        }
    }

    // 通用动作守卫：非 push 的需登录操作（tab 切换等）未登录时先弹应用内登录页，
    // 登录成功后执行 callback；登录页被关闭则丢弃回调
    function guardAction(callback) {
        if (!authViewModel.loggedIn) {
            pendingNav = {kind: "action", callback: callback}
            openInAppLogin()
        } else if (callback) {
            callback()
        }
    }

    // 打开应用内登录页（幂等：登录页已在栈顶则不重复压层；连点多个需登录操作只弹一层）
    function openInAppLogin() {
        if (loginPageOpen) return
        loginPageOpen = true
        stackView.push(loginPage, {inAppMode: true})
    }

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
        // 请求守卫：存在未登录时被挂起的 Interactive 请求 → 弹应用内登录页（全屏模态）
        // 防重入：与 guardedPush/guardAction 共用 loginPageOpen，登录页只压一层
        function onAuthRequired() {
            openInAppLogin()
        }
        // 401 兜底：已登录用户 token 失效 → AuthViewModel 登出 → onLoggedInChanged 转游客模式继续浏览；
        // 游客的请求已被请求守卫拦截（不会发出），因此 401 不会打断游客浏览
    }

    StackView {
        id: stackView
        anchors.fill: parent
        // 未登录也进入主界面（游客模式）：登录页仅首次启动弹欢迎或需登录时按需弹出
        initialItem: isLoading ? loadingComponent : homePage

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
        LoginPage {
            // welcomeMode/inAppMode 属性由 LoginPage 自身声明，push 时传入
            // 欢迎页跳过 → 进入游客模式
            onSkipRequested: {
                pendingNav = null
                loginPageOpen = false
                stackView.replace(homePage)
            }
            // 应用内登录页关闭 → 取消被守卫拦下的导航/动作与挂起请求，回到原页面
            onCloseRequested: {
                pendingNav = null
                loginPageOpen = false
                httpApi.cancelAuthQueue()
                stackView.pop()
            }
        }
    }

    Component {
        id: homePage
        HomePage {
            onShowDetailRequest: (recipeId) => {
                guardedPush(recipeDetailPage, {recipeId: recipeId})
            }
            onShowSubmitRequest: () => {
                coverImageDialog.openWithFilter(qsTr("选择封面图片"), "图片文件 (*.jpg *.jpeg *.png *.gif *.bmp *.svg)")
            }
            onShowMyRecipesRequest: () => {
                guardedPush(myRecipesPage, undefined, true)
            }
            onShowMyRatingsRequest: () => {
                guardedPush(myRatingsPage, undefined, true)
            }
            onShowSearchRequest: () => {
                guardedPush(searchPage)
            }
            onShowRecommendFromInventory: () => {
                guardedPush(recommendResultsPage, undefined, true)
            }
            onShowSettingsRequest: () => {
                guardedPush(settingsPage, undefined, true)
            }
            onShowNotificationRequest: () => {
                guardedPush(notificationPage, undefined, true)
            }
            onShowShoppingListRequest: () => {
                guardedPush(shoppingListPage, undefined, true)
            }
            onShowLoginRequest: () => {
                // 游客在"我的"页点登录：弹应用内登录页，成功后在原页面继续
                guardAction(function() { })
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
            guardedPush(submitRecipePage, { coverImagePath: path, _stackView: stackView }, true)
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
                guardedPush(recipeDetailPage, {recipeId: recipeId})
            }
        }
    }

    Component {
        id: recommendResultsPage
        RecommendResultsPage {
            property var _stackView: stackView
            onRecipeClicked: (recipeId, healthNotice) => {
                guardedPush(recipeDetailPage, {recipeId: recipeId, healthNotice: healthNotice || ""})
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
                guardedPush(shoppingListDetailPage, {listId: listId}, true)
            }
        }
    }

    Component {
        id: profileEditPage
        ProfileEditPage {
        }
    }

    Component {
        id: settingsPage
        SettingsPage {
            onEditProfileRequest: () => {
                guardedPush(profileEditPage, undefined, true)
            }
            onAccountSecurityRequest: () => {
                guardedPush(accountSecurityPage, undefined, true)
            }
            onDietaryPreferencesRequest: () => {
                guardedPush(preferencesPage, undefined, true)
            }
            onHealthProfileRequest: () => {
                guardedPush(healthProfilePage, undefined, true)
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
        PreferencesPage {
        }
    }

    Component {
        id: healthProfilePage
        HealthProfilePage {
        }
    }

    Component {
        id: changePasswordPage
        ChangePasswordPage {
        }
    }

    Component {
        id: accountSecurityPage
        AccountSecurityPage {
            onShowChangePasswordRequest: () => {
                guardedPush(changePasswordPage, undefined, true)
            }
        }
    }

    Component {
        id: notificationPage
        NotificationPage {
            property var _stackView: stackView
            onShowDetailRequest: (data) => {
                guardedPush(notificationDetailPage, {notificationData: data}, true)
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
            if (authViewModel.initialLoading) return
            if (authViewModel.loggedIn) {
                stackView.replace(homePage)
            } else {
                // 未登录启动：弹欢迎登录页（可跳过进入游客模式）。
                // 每次启动时只要未登录都会弹——退出时处于未登录状态，下次打开必然见到欢迎页
                stackView.replace(loginPage, {welcomeMode: true})
            }
        }
        function onLoggedInChanged() {
            if (authViewModel.initialLoading) return
            if (authViewModel.loggedIn) {
                if (pendingNav) {
                    // 应用内登录成功：消费被守卫拦下的意图（登录页此时在栈顶）
                    var p = pendingNav
                    pendingNav = null
                    loginPageOpen = false
                    if (p.kind === "push") {
                        // 用目标页替换登录页，继续之前被拦下的导航
                        stackView.replace(p.component, p.props)
                    } else {
                        // 动作守卫：pop 回原页面并执行挂起的动作
                        stackView.pop()
                        if (p.callback) p.callback()
                    }
                } else if (loginPageOpen) {
                    // 请求守卫登录成功：pop 回原页面（挂起请求已由 tokenChanged 自动重放）
                    loginPageOpen = false
                    stackView.pop()
                } else {
                    // 欢迎页登录成功
                    stackView.replace(homePage)
                }
            } else {
                // 登出 / token 失效：清空全部守卫残留状态（防陈旧意图在下一次登录时误执行、
                // authRequired 卡死不再弹登录页），并清空上一账号的库存/收藏数据，进入游客模式继续浏览
                pendingNav = null
                loginPageOpen = false
                httpApi.cancelAuthQueue()
                inventoryVM.clearAll()
                recipeVM.clearFavorites()
                stackView.replace(homePage)
            }
        }
    }
}
