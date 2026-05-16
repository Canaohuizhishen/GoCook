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
    signal showSearchRequest()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 搜索栏（仅在推荐页显示，点击进入搜索页）
        Rectangle {
            visible: swipeView.currentIndex === 0
            Layout.fillWidth: true
            Layout.margins: Theme.spacingMedium
            Layout.topMargin: Theme.spacingMedium
            Layout.bottomMargin: Theme.spacingSmall
            height: 46
            color: Theme.isDarkMode ? "#2A2A2A" : "#EEEEEE"
            radius: Theme.radiusLarge

            Row {
                anchors.centerIn: parent
                spacing: 8

                Canvas {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.strokeStyle = Theme.textHint
                        ctx.lineWidth = 1.5
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.ellipse(0.5, 0.5, 10, 10)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(9, 9)
                        ctx.lineTo(14.5, 14.5)
                        ctx.stroke()
                    }
                }

                Label {
                    text: qsTr("搜索菜谱、食材...")
                    color: Theme.textHint
                    font.pixelSize: Theme.fontSizeBody
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: homePage.showSearchRequest()
            }
        }

        SwipeView {
            id: swipeView
            Layout.fillWidth: true
            Layout.fillHeight: true
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
}
