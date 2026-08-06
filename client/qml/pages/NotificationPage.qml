import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: notificationPage
    title: qsTr("消息通知")

    signal showDetailRequest(var data)

    property string errorMessage: ""

    // 页面可见时加载数据
    onVisibleChanged: {
        if (visible)
            notifyVM.refresh()
    }

    Component.onCompleted: {
        if (visible)
            notifyVM.refresh()
    }

    // ========== 顶部标题栏（返回按钮 + 标题 + 全部已读） ==========
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 48
        color: "transparent"

        // 返回按钮
        Button {
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 40; height: 40
            flat: true
            contentItem: Canvas {
                width: 22
                height: 22
                property color arrowColor: Theme.textPrimary
                onArrowColorChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.strokeStyle = arrowColor
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(14, 5)
                    ctx.lineTo(6, 11)
                    ctx.lineTo(14, 17)
                    ctx.stroke()
                }
            }
            onClicked: {
                if (_stackView) _stackView.pop()
            }
        }

        Label {
            anchors.centerIn: parent
            text: qsTr("消息通知")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH2
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }

        // 全部已读按钮
        Button {
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingMedium
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("全部已读")
            flat: true
            visible: notifyVM.unreadCount > 0
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            onClicked: notifyVM.markAllRead()
        }
    }

    // 错误/成功提示（底部 toast 风格，1.5 秒自动消失；替代原顶部红条）
    ErrorBanner {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 48
        anchors.horizontalCenter: parent.horizontalCenter
        text: errorMessage
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: 48

        Column {
            anchors.fill: parent

            // ========== 类型筛选标签栏（可横向滑动） ==========
            Rectangle {
                width: parent.width
                height: 44
                color: Theme.cardBackground
                border.color: Theme.dividerColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingSmall
                    anchors.rightMargin: Theme.spacingSmall
                    spacing: Theme.spacingXSmall

                    Button {
                        text: qsTr("全部")
                        flat: true
                        Layout.preferredHeight: 30
                        highlighted: notifyVM.currentType === ""
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        leftPadding: 12; rightPadding: 12
                        topPadding: 0; bottomPadding: 0
                        background: Rectangle {
                            radius: 6
                            color: parent.highlighted ? Theme.primaryColor
                                 : parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.25)
                                 : parent.hovered ? Qt.rgba(0, 0, 0, 0.06)
                                 : Theme.searchBarBackground
                            Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                        }
                        onClicked: notifyVM.currentType = ""
                    }

                    Button {
                        text: qsTr("系统公告")
                        flat: true
                        Layout.preferredHeight: 30
                        highlighted: notifyVM.currentType === "system"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        leftPadding: 12; rightPadding: 12
                        topPadding: 0; bottomPadding: 0
                        background: Rectangle {
                            radius: 6
                            color: parent.highlighted ? Theme.primaryColor
                                 : parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.25)
                                 : parent.hovered ? Qt.rgba(0, 0, 0, 0.06)
                                 : Theme.searchBarBackground
                            Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                        }
                        onClicked: notifyVM.currentType = "system"
                    }

                    Button {
                        text: qsTr("审核结果")
                        flat: true
                        Layout.preferredHeight: 30
                        highlighted: notifyVM.currentType === "review"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        leftPadding: 12; rightPadding: 12
                        topPadding: 0; bottomPadding: 0
                        background: Rectangle {
                            radius: 6
                            color: parent.highlighted ? Theme.primaryColor
                                 : parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.25)
                                 : parent.hovered ? Qt.rgba(0, 0, 0, 0.06)
                                 : Theme.searchBarBackground
                            Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                        }
                        onClicked: notifyVM.currentType = "review"
                    }

                    Button {
                        text: qsTr("互动提醒")
                        flat: true
                        Layout.preferredHeight: 30
                        highlighted: notifyVM.currentType === "interaction"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        leftPadding: 12; rightPadding: 12
                        topPadding: 0; bottomPadding: 0
                        background: Rectangle {
                            radius: 6
                            color: parent.highlighted ? Theme.primaryColor
                                 : parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.25)
                                 : parent.hovered ? Qt.rgba(0, 0, 0, 0.06)
                                 : Theme.searchBarBackground
                            Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                        }
                        onClicked: notifyVM.currentType = "interaction"
                    }
                }
            }

            // ========== 通知列表 ==========
            Rectangle {
                width: parent.width
                height: parent.height - 44
                color: Theme.backgroundColor

                // 下拉刷新指示器
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 8
                    width: 24; height: 24
                    radius: 12
                    color: Theme.primaryColor
                    visible: notifyVM.isRefreshing
                    z: 10

                    Text {
                        anchors.centerIn: parent
                        text: "\u21bb"
                        color: "white"
                        font.pointSize: 14
                    }
                }

                LoadingIndicator {
                    id: loadingIndicator
                    fullscreen: true
                    message: qsTr("正在加载通知...")
                    isLoading: notifyVM.isLoading && notifyVM.notifications.length === 0
                }

                ListView {
                    id: notificationListView
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSmall
                    spacing: Theme.spacingSmall
                    clip: true
                    visible: notifyVM.notifications.length > 0

                    model: notifyVM.notifications

                    delegate: NotificationCardDelegate {
                        width: notificationListView.width
                        notificationId: modelData.id
                        notifTitle: modelData.title || ""
                        notifContent: modelData.content || ""
                        createdAt: modelData.createdAt || ""
                        notifType: modelData.type || ""
                        isRead: modelData.is_read || false
                        onReadRequested: function(id) { notifyVM.markRead(id) }
                        onDeleteRequested: function(id) { notifyVM.deleteNotification(id) }
                        onClicked: function(data) { notificationPage.showDetailRequest(data) }
                    }
                    // 触底加载更多
                    onAtYEndChanged: {
                        if (atYEnd && !notifyVM.isLoading && notifyVM.hasMore)
                            notifyVM.loadNextPage()
                    }
                }

                // ========== 空状态 ==========
                Column {
                    anchors.centerIn: parent
                    width: Math.min(320, parent.width * 0.85)
                    spacing: Theme.spacingMedium
                    visible: notifyVM.notifications.length === 0 && !notifyVM.isLoading

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "\uD83D\uDCED"
                        font.pointSize: 48
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("暂无通知")
                        color: Theme.textHint
                        font.pointSize: Theme.fontSizeBody
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("当有系统公告、审核结果或互动提醒时，将在这里显示")
                        color: Theme.textHint
                        font.pointSize: Theme.fontSizeCaption
                        opacity: 0.6
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        width: parent.width/1.5
                    }
                }
            }
        }
    }

    // ========== 连接到 ViewModel 的信号 ==========
    Connections {
        target: notifyVM
        function onErrorOccurred(error) {
            errorMessage = ""
            errorMessage = error
        }
        function onMarkReadSuccess(id) {
            // 可选：显示短暂提示
        }
        function onMarkAllReadSuccess() {
            errorMessage = ""
            errorMessage = qsTr("全部标记为已读")
        }
        function onDeleteSuccess(id) {
            // 删除成功，列表已自动更新
        }
    }
}
