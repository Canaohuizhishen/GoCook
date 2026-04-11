import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: homePage
    title: qsTr("首页")

    // 头部工具栏（可选）
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                text: stackView.currentItem ? stackView.currentItem.title : "GoCook"
                font.pixelSize: 20
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
        }
    }

    // 底部导航栏
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

    // 滑动视图，与 TabBar 联动
    SwipeView {
        id: swipeView
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        // 推荐页面
        RecommendPage {
            title: qsTr("推荐")
        }

        // 库存页面
        InventoryPage {
            title: qsTr("我的库存")
        }

        // 个人中心页面
        ProfilePage {
            title: qsTr("个人中心")
        }
    }
}