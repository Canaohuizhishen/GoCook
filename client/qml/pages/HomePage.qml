import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    id: homePage
    title: qsTr("首页")

    signal showDetailRequest(int recipeId)
    signal showSubmitRequest()

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                Layout.fillWidth: true
                text: qsTr("GoCook")
                font.pixelSize: 20
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
            }
        }
    }

    footer: TabBar {
        id: tabBar
        currentIndex: swipeView.currentIndex

        TabButton {
            text: qsTr("推荐")
        }
        TabButton {
            text: qsTr("库存")
        }
        TabButton {
            text: qsTr("我的")
        }
    }

    SwipeView {
        id: swipeView
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        RecommendPage {
            title: qsTr("推荐")
            onRecipeClicked: (recipeId) => {
                homePage.showDetailRequest(recipeId)
            }
        }

        InventoryPage {
            title: qsTr("我的库存")
        }

        ProfilePage {
            title: qsTr("个人中心")
            onShowSubmitRequest: {
                homePage.showSubmitRequest()
            }
        }
    }
}
