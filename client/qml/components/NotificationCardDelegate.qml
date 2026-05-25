import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client

Item {
    id: root
    width: parent ? parent.width : 300
    height: 100

    property int notificationId: 0
    property string notifTitle: ""
    property string notifContent: ""
    property string createdAt: ""
    property string notifType: ""
    property bool isRead: false

    signal clicked(var data)
    signal deleteRequested(int id)
    signal readRequested(int id)

    property real swipeOffset: 0
    property bool deleting: false

    Timer {
        id: deleteTimer
        interval: 250
        onTriggered: {
            root.deleting = false
            root.swipeOffset = 0
            root.deleteRequested(root.notificationId)
        }
    }

    Behavior on swipeOffset {
        NumberAnimation {
            duration: 220
            easing.type: Easing.OutCubic
        }
    }

    // 红色删除区
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 80
        color: "#E74C3C"
        visible: root.swipeOffset < -2

        Text {
            anchors.centerIn: parent
            text: qsTr("删除")
            color: "white"
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            font.weight: Font.Bold
        }
    }

    // 内容卡片
    Rectangle {
        id: contentCard
        x: root.swipeOffset
        width: parent.width
        height: parent.height
        radius: Theme.radiusLarge
        color: root.isRead ? Theme.cardBackground : Qt.rgba(0.42, 0.53, 0.85, 0.06)
        clip: true

        Item {
            id: cardBody
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Theme.spacingMedium
            }
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

                // 标题行
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

                // 时间
                Text {
                    text: {
                        var d = root.createdAt || ""
                        if (d.length >= 16)
                            return d.substring(0, 10) + " " + d.substring(11, 16)
                        return d.substring(0, 10)
                    }
                    width: textBody.width
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeSmall
                    color: Theme.textHint
                }
            }
        }

        // 手势处理
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            property real startX: 0
            property bool isSwiping: false

            onPressed: function(mouse) {
                if (root.deleting) return
                startX = mouse.x
                isSwiping = false
            }
            onPositionChanged: function(mouse) {
                if (root.deleting) return
                var dx = mouse.x - startX
                if (dx < -10) {
                    isSwiping = true
                    root.swipeOffset = Math.max(-80, dx)
                } else if (dx > 2 && root.swipeOffset < 0) {
                    root.swipeOffset = Math.min(0, dx)
                }
            }
            onClicked: function(mouse) {
                if (isSwiping || root.deleting) return
                if (!root.isRead)
                    root.readRequested(root.notificationId)
                root.clicked({
                    id: root.notificationId,
                    title: root.notifTitle,
                    content: root.notifContent,
                    type: root.notifType,
                    createdAt: root.createdAt,
                    is_read: root.isRead
                })
            }
            onReleased: function(mouse) {
                if (!isSwiping || root.deleting) return
                if (root.swipeOffset < -40) {
                    root.deleting = true
                    root.swipeOffset = -root.width
                    deleteTimer.start()
                } else {
                    root.swipeOffset = 0
                }
            }
        }

        // 底部分隔线
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.dividerColor
            opacity: 0.3
        }
    }
}
