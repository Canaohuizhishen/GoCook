import QtQuick
import client

// 通知列表项（v2.23）——左滑显露式删除（复用标准组件 SwipeToDeleteItem）：
//   左滑仅显露删除按钮（点击才删除，不再松手即删）；点内容区：未展开 → 进详情，展开 → 收起；
//   同一时刻仅一项展开（由页面共享的 swipeManager 互斥）；水平拖动时锁定列表纵向滚动。
// 外部契约：notificationId/notifTitle/notifContent/createdAt/notifType/isRead
//          + clicked(data)（进详情）+ deleteRequested()（基类信号；页面用 modelData.id 执行删除）。
SwipeToDeleteItem {
    id: root

    implicitHeight: 100   // 卡片视觉高度（删除时由列表数据源移除，无收缩动画）

    property int notificationId: 0
    property string notifTitle: ""
    property string notifContent: ""
    property string createdAt: ""
    property string notifType: ""
    property bool isRead: false

    signal clicked(var data)

    // 点内容区（未展开时）→ 进详情；展开态的"点即收起"由组件内部消化
    onContentClicked: root.clicked({
        id: root.notificationId,
        title: root.notifTitle,
        content: root.notifContent,
        type: root.notifType,
        createdAt: root.createdAt,
        is_read: root.isRead
    })

    // ========== 卡片内容（放入组件的可滑动内容区；卡面底色/圆角由组件提供） ==========
    Item {
        anchors.fill: parent
        anchors.rightMargin: Theme.spacingMedium

        Item {
            id: cardBody
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 14
            height: Math.max(iconRect.height, textBody.height)

            // 类型图标
            Rectangle {
                id: iconRect
                anchors { left: parent.left; top: parent.top }
                width: 36
                height: 36
                radius: 18
                color: {
                    if (root.notifType === "system") return Theme.primaryColor
                    if (root.notifType === "review") return "#F39C12"
                    if (root.notifType === "interaction") return "#2ECC71"
                    return Theme.textHint
                }

                Text {
                    anchors.centerIn: parent
                    text: {
                        if (root.notifType === "system") return "\uD83D\uDCE2"
                        if (root.notifType === "review") return "\u270F"
                        if (root.notifType === "interaction") return "\uD83D\uDCAC"
                        return "\uD83D\uDD14"
                    }
                    font.pointSize: 16
                }
            }

            // 文字内容
            Column {
                id: textBody
                anchors {
                    left: iconRect.right
                    leftMargin: Theme.spacingMedium
                    right: parent.right
                    top: parent.top
                }
                spacing: Theme.spacingXSmall

                // 标题行（未读加粗 + 圆点）
                Row {
                    width: textBody.width
                    spacing: Theme.spacingXSmall

                    Text {
                        text: root.notifTitle
                        width: parent.width - (root.isRead ? 0 : 16)
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        font.weight: root.isRead ? Font.Normal : Font.Bold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: Theme.primaryColor
                        visible: !root.isRead
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // 内容摘要
                Text {
                    text: root.notifContent
                    width: textBody.width
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeCaption
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                // 时间（微信式相对格式：今天 HH:mm / 昨天 / MM-DD）
                Text {
                    text: Theme.formatRelativeTime(root.createdAt)
                    width: textBody.width
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeSmall
                    color: Theme.textHint
                }
            }
        }
    }
}
