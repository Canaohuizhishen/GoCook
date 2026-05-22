import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: notificationPage
    title: qsTr("消息通知")

    background: Rectangle { color: Theme.backgroundColor }

    signal showDetailRequest(var data)

    // F5 重置测试通知（开发用）
    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_F5) {
            notifyVM.resetTestData()
            event.accepted = true
        }
    }

    property string errorMessage: ""

    // 错误提示自动消失
    Timer {
        id: errorTimer
        interval: 3000
        onTriggered: errorMessage = ""
    }

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
            contentItem: Text {
                text: "\u2190"
                font.pointSize: 22
                color: Theme.primaryColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
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
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: Theme.primaryColor
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: notifyVM.markAllRead()
        }
    }

    // 错误提示
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 48
        width: parent.width
        height: 32
        color: "#E74C3C"
        visible: errorMessage.length > 0

        Text {
            anchors.centerIn: parent
            text: errorMessage
            color: "white"
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
        }
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

                Flickable {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingSmall
                    contentWidth: row.width
                    contentHeight: parent.height
                    flickableDirection: Flickable.HorizontalFlick
                    interactive: true
                    clip: true

                    Row {
                        id: row
                        y: Math.max(0, (parent.height - height) / 2)
                        spacing: Theme.spacingXSmall

                        Button {
                            text: qsTr("全部")
                            flat: true
                            height: 30
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            leftPadding: 12; rightPadding: 12
                            topPadding: 0; bottomPadding: 0
                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: notifyVM.currentType === "" ? Theme.primaryColor : Theme.searchBarBackground
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: notifyVM.currentType === "" ? "white" : Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: notifyVM.currentType = ""
                        }

                        Button {
                            text: qsTr("系统公告")
                            flat: true
                            height: 30
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            leftPadding: 12; rightPadding: 12
                            topPadding: 0; bottomPadding: 0
                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: notifyVM.currentType === "system" ? Theme.primaryColor : Theme.searchBarBackground
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: notifyVM.currentType === "system" ? "white" : Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: notifyVM.currentType = "system"
                        }

                        Button {
                            text: qsTr("审核结果")
                            flat: true
                            height: 30
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            leftPadding: 12; rightPadding: 12
                            topPadding: 0; bottomPadding: 0
                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: notifyVM.currentType === "review" ? Theme.primaryColor : Theme.searchBarBackground
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: notifyVM.currentType === "review" ? "white" : Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: notifyVM.currentType = "review"
                        }

                        Button {
                            text: qsTr("互动提醒")
                            flat: true
                            height: 30
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            leftPadding: 12; rightPadding: 12
                            topPadding: 0; bottomPadding: 0
                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: notifyVM.currentType === "interaction" ? Theme.primaryColor : Theme.searchBarBackground
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: notifyVM.currentType === "interaction" ? "white" : Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: notifyVM.currentType = "interaction"
                        }
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
                    spacing: 1
                    clip: true
                    visible: notifyVM.notifications.length > 0

                    model: notifyVM.notifications

                    delegate: Item {
                        id: delegateRoot
                        width: notificationListView.width
                        height: 80

                        // 滑动偏移量（负值=向左滑出，仅用于删除）
                        property real swipeOffset: 0
                        property bool deleting: false

                        Timer {
                            id: deleteTimer
                            interval: 250
                            onTriggered: {
                                delegateRoot.deleting = false
                                delegateRoot.swipeOffset = 0
                                notifyVM.deleteNotification(modelData.id)
                            }
                        }

                        Behavior on swipeOffset {
                            NumberAnimation {
                                duration: 220
                                easing.type: Easing.OutCubic
                            }
                        }

                        // ========== 两层结构：内容卡片左移 + 红色删除区固定右侧 ==========
                        // 红色删除区（固定在右侧，始终在 delegateRoot 范围内）
                        Rectangle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 80
                            color: "#E74C3C"
                            visible: delegateRoot.swipeOffset < -2

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("删除")
                                color: "white"
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeCaption
                                font.weight: Font.Bold
                            }
                        }

                        // 内容卡片（全宽，通过 x 左移露出红色删除区）
                        Rectangle {
                            id: contentCard
                            x: delegateRoot.swipeOffset
                            width: parent.width
                            height: parent.height
                            radius: Theme.radiusMedium
                            color: modelData.is_read ? Theme.cardBackground : Qt.rgba(0.42, 0.53, 0.85, 0.06)
                            clip: true

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                spacing: Theme.spacingMedium

                                // 类型图标
                                Rectangle {
                                    Layout.preferredWidth: 36
                                    Layout.preferredHeight: 36
                                    Layout.alignment: Qt.AlignTop
                                    radius: 18
                                    color: {
                                        if (modelData.type === "system") return Theme.primaryColor
                                        if (modelData.type === "review") return "#F39C12"
                                        if (modelData.type === "interaction") return "#2ECC71"
                                        return Theme.textHint
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: {
                                            if (modelData.type === "system") return "\uD83D\uDCE2"
                                            if (modelData.type === "review") return "\u270F"
                                            if (modelData.type === "interaction") return "\uD83D\uDCAC"
                                            return "\uD83D\uDD14"
                                        }
                                        font.pointSize: 16
                                    }
                                }

                                // 文字内容
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingXSmall

                                        Text {
                                            text: modelData.title || ""
                                            font.family: Theme.fontFamily
                                            font.pointSize: Theme.fontSizeBody
                                            font.weight: modelData.is_read ? Font.Normal : Font.Bold
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Rectangle {
                                            width: 8; height: 8
                                            radius: 4
                                            color: Theme.primaryColor
                                            visible: !modelData.is_read
                                            Layout.alignment: Qt.AlignTop
                                            Layout.topMargin: 4
                                        }
                                    }

                                    Text {
                                        text: modelData.content || ""
                                        font.family: Theme.fontFamily
                                        font.pointSize: Theme.fontSizeCaption
                                        color: Theme.textSecondary
                                        elide: Text.ElideRight
                                        maximumLineCount: 2
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: {
                                            var date = modelData.createdAt || ""
                                            if (date.length > 10)
                                                return date.substring(0, 10) + " " + date.substring(11, 19)
                                            return date
                                        }
                                        font.family: Theme.fontFamily
                                        font.pointSize: Theme.fontSizeSmall
                                        color: Theme.textHint
                                    }
                                }
                            }

                            // ========== 手势处理 ==========
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                property real startX: 0
                                property bool isSwiping: false

                                onPressed: function(mouse) {
                                    if (delegateRoot.deleting) return
                                    startX = mouse.x
                                    isSwiping = false
                                }
                                onPositionChanged: function(mouse) {
                                    if (delegateRoot.deleting) return
                                    var dx = mouse.x - startX
                                    if (dx < -10) {
                                        isSwiping = true
                                        delegateRoot.swipeOffset = Math.max(-80, dx)
                                    } else if (dx > 2 && delegateRoot.swipeOffset < 0) {
                                        delegateRoot.swipeOffset = Math.min(0, dx)
                                    }
                                }
                                onClicked: function(mouse) {
                                    if (isSwiping || delegateRoot.deleting) return
                                    if (!modelData.is_read)
                                        notifyVM.markRead(modelData.id)
                                    notificationPage.showDetailRequest(modelData)
                                }
                                onReleased: function(mouse) {
                                    if (!isSwiping || delegateRoot.deleting) return
                                    if (delegateRoot.swipeOffset < -40) {
                                        delegateRoot.deleting = true
                                        delegateRoot.swipeOffset = -delegateRoot.width
                                        deleteTimer.start()
                                    } else {
                                        delegateRoot.swipeOffset = 0
                                    }
                                }
                            }

                            // 分隔线（在内容卡片底部）
                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: Theme.dividerColor
                                opacity: 0.3
                            }
                        }
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
                        width: parent.width * 0.8
                    }
                }
            }
        }
    }

    // ========== 连接到 ViewModel 的信号 ==========
    Connections {
        target: notifyVM
        function onErrorOccurred(error) {
            errorMessage = error
            errorTimer.restart()
        }
        function onMarkReadSuccess(id) {
            // 可选：显示短暂提示
        }
        function onMarkAllReadSuccess() {
            errorMessage = qsTr("全部标记为已读")
            errorTimer.restart()
        }
        function onDeleteSuccess(id) {
            // 删除成功，列表已自动更新
        }
    }
}
