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
        HomePage {
            onShowDetailRequest: (recipeId) => {
                stackView.push(recipeDetailPage, {recipeId: recipeId})
            }
            onShowSubmitRequest: () => {
                stackView.push(submitRecipePage)
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
        id: loadingComponent
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
