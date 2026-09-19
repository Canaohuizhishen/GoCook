import QtQuick
import client

// 系统通知（公告）列表项（v2.23）——变高卡片（内容多行自适应）+ 左滑显露式删除：
//   复用标准组件 SwipeToDeleteItem（左滑仅显露删除按钮、点击才删、互斥单开、点内容收起）。
//   注：公告为公共资源、服务端无删除接口——点「删除」= 会话级本地隐藏
//   （详见 AnnouncementViewModel::hideAnnouncement：同会话刷新不复活，重启/登出后恢复）。
// 外部契约：announcementId/annTitle/annContent/annTime + clicked(data) + deleteRequested()（基类信号）。
SwipeToDeleteItem {
    id: root

    property int announcementId: 0
    property string annTitle: ""
    property string annContent: ""
    property string annTime: ""

    signal clicked(var data)

    // 变高：内容自适应（删除时由列表数据源移除，无收缩动画）
    implicitHeight: contentColumn.height + Theme.spacingMedium * 2

    onContentClicked: root.clicked({
        id: root.announcementId,
        title: root.annTitle,
        content: root.annContent,
        createdAt: root.annTime
    })

    // ========== 卡面内容（放入组件的可滑动内容区；卡面底色/圆角由组件提供） ==========
    Item {
        anchors.fill: parent
        anchors.rightMargin: Theme.spacingMedium

        // 左侧强调条（公告视觉标识）
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.spacingSmall
            anchors.bottomMargin: Theme.spacingSmall
            width: 3
            radius: 1.5
            color: Theme.primaryColor
        }

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingSmall + 2
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: Theme.spacingMedium
            spacing: Theme.spacingXSmall

            // 标题行 + 「公告」标识
            Row {
                width: parent.width
                spacing: Theme.spacingXSmall

                Text {
                    text: root.annTitle
                    width: Math.min(implicitWidth, parent.width - 44)
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Rectangle {
                    width: 36; height: 18
                    radius: 9
                    color: Theme.primaryColor

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("公告")
                        color: "white"
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall
                    }
                }
            }

            // 内容（多行自适应 —— 条目高低可不同）
            Text {
                text: root.annContent
                width: parent.width
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            // 时间（微信式相对格式：今天 HH:mm / 昨天 / MM-DD）
            Text {
                text: Theme.formatRelativeTime(root.annTime)
                width: parent.width
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeSmall
                color: Theme.textHint
            }
        }
    }
}
