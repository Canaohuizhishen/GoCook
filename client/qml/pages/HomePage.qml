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
    signal showShoppingListRequest()
    signal showAnnouncementsRequest()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 搜索栏（仅在首页显示，点击进入搜索页）
        Rectangle {
            visible: swipeView.currentIndex === 0
            Layout.fillWidth: true
            Layout.margins: Theme.spacingMedium
            Layout.topMargin: Theme.spacingMedium
            Layout.bottomMargin: Theme.spacingSmall
            height: 40
            color: Theme.searchBarBackground
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
                    font.pointSize: Theme.fontSizeBody
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
                onRecipeClicked: (recipeId) => {
                    homePage.showDetailRequest(recipeId)
                }
            }

            InventoryPage {
                onShowShoppingListRequest: {
                    homePage.showShoppingListRequest()
                }
            }

            FavoritesPage {
            }

            ProfilePage {
                onShowSubmitRequest: {
                    homePage.showSubmitRequest()
                }
                onShowAnnouncementsRequest: {
                    homePage.showAnnouncementsRequest()
                }
            }
        }
    }

    footer: TabBar {
        id: tabBar
        currentIndex: swipeView.currentIndex
        contentHeight: 50

        TabButton {
            id: homeTab
            topPadding: 2
            bottomPadding: 2
            contentItem: Column {
                spacing: 1
                anchors.centerIn: parent
                Canvas {
                    width: Theme.tabIconSize
                    height: Theme.tabIconSize
                    anchors.horizontalCenter: parent.horizontalCenter
                    property color icoColor: homeTab.checked ? Theme.primaryColor : Theme.textHint
                    onIcoColorChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.strokeStyle = icoColor
                        ctx.lineWidth = 1.5
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        ctx.beginPath()
                        ctx.moveTo(2, height * 0.52)
                        ctx.lineTo(width / 2, 3)
                        ctx.lineTo(width - 2, height * 0.52)
                        ctx.stroke()
                        ctx.strokeRect(4, height * 0.52, width - 8, height * 0.4)
                    }
                }
                Label {
                    text: qsTr("首页")
                    font.pointSize: 9
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: homeTab.checked ? Theme.primaryColor : Theme.textHint
                }
            }
        }

        TabButton {
            id: invTab
            topPadding: 2
            bottomPadding: 2
            contentItem: Column {
                spacing: 1
                anchors.centerIn: parent
                Canvas {
                    width: Theme.tabIconSize
                    height: Theme.tabIconSize
                    anchors.horizontalCenter: parent.horizontalCenter
                    property color icoColor: invTab.checked ? Theme.primaryColor : Theme.textHint
                    onIcoColorChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.strokeStyle = icoColor
                        ctx.lineWidth = 1.5
                        ctx.strokeRect(3, 5, width - 6, height - 8)
                        ctx.beginPath()
                        ctx.moveTo(3, 5)
                        ctx.lineTo(width / 2, 2)
                        ctx.lineTo(width - 3, 5)
                        ctx.stroke()
                    }
                }
                Label {
                    text: qsTr("库存")
                    font.pointSize: 9
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: invTab.checked ? Theme.primaryColor : Theme.textHint
                }
            }
        }

        TabButton {
            id: favTab
            topPadding: 2
            bottomPadding: 2
            contentItem: Column {
                spacing: 1
                anchors.centerIn: parent
                Canvas {
                    width: Theme.tabIconSize
                    height: Theme.tabIconSize
                    anchors.horizontalCenter: parent.horizontalCenter
                    property color icoColor: favTab.checked ? Theme.primaryColor : Theme.textHint
                    onIcoColorChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.strokeStyle = icoColor
                        ctx.lineWidth = 1.5
                        ctx.lineJoin = "round"
                        var cx = width / 2, cy = height / 2
                        var outerR = width / 2 - 1, innerR = outerR * 0.38
                        ctx.beginPath()
                        for (var i = 0; i < 5; i++) {
                            var outerAngle = -Math.PI / 2 + i * (2 * Math.PI / 5)
                            var innerAngle = outerAngle + Math.PI / 5
                            var ox = cx + outerR * Math.cos(outerAngle)
                            var oy = cy + outerR * Math.sin(outerAngle)
                            var ix = cx + innerR * Math.cos(innerAngle)
                            var iy = cy + innerR * Math.sin(innerAngle)
                            if (i === 0) ctx.moveTo(ox, oy)
                            else ctx.lineTo(ox, oy)
                            ctx.lineTo(ix, iy)
                        }
                        ctx.closePath()
                        ctx.stroke()
                    }
                }
                Label {
                    text: qsTr("收藏")
                    font.pointSize: 9
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: favTab.checked ? Theme.primaryColor : Theme.textHint
                }
            }
        }

        TabButton {
            id: profileTab
            topPadding: 2
            bottomPadding: 2
            contentItem: Column {
                spacing: 1
                anchors.centerIn: parent
                Canvas {
                    width: Theme.tabIconSize
                    height: Theme.tabIconSize
                    anchors.horizontalCenter: parent.horizontalCenter
                    property color icoColor: profileTab.checked ? Theme.primaryColor : Theme.textHint
                    onIcoColorChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.strokeStyle = icoColor
                        ctx.lineWidth = 1.5
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2 - 2, 0, Math.PI * 2)
                        ctx.stroke()
                    }
                }
                Label {
                    text: qsTr("我的")
                    font.pointSize: 9
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: profileTab.checked ? Theme.primaryColor : Theme.textHint
                }
            }
        }
    }
}
