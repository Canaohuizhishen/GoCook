import QtQuick
import QtQuick.Controls
import client
import "../components"

// 系统通知（公告）独立页（v2.23）：
//   进入页面即上报水位，红点消失；无「全部已读」；列表禁止越界下拉、空态在内容流中（footer）。
//   条目高低不一（内容多行自适应）+ 左滑显露式删除（会话级本地隐藏，详见 AnnouncementViewModel）；
//   点击进入详情（复用通知详情页）。
Page {
    id: systemNoticePage
    title: qsTr("系统通知")

    signal showDetailRequest(var data)

    property string errorMessage: ""

    // 左滑互斥管理器：同一时刻仅一个条目处于"显露删除按钮"态（SwipeToDeleteItem 契约）
    QtObject {
        id: noticeSwipeState
        property Item currentItem: null
    }

    onVisibleChanged: {
        if (visible)
            announcementVM.refresh()
    }

    Component.onCompleted: {
        if (visible)
            announcementVM.refresh()
    }

    // ========== 顶部标题栏（返回按钮 + 标题） ==========
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 48
        color: "transparent"

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
            text: qsTr("系统通知")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH2
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }
    }

    ErrorBanner {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 48
        anchors.horizontalCenter: parent.horizontalCenter
        text: errorMessage
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: 48

        // 下拉刷新指示器
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 8
            width: 24; height: 24
            radius: 12
            color: Theme.primaryColor
            visible: announcementVM.isRefreshing
            z: 10

            NumberAnimation on rotation {
                from: 0; to: 360
                duration: 800
                loops: Animation.Infinite
                running: announcementVM.isRefreshing
            }

            Text {
                anchors.centerIn: parent
                text: "\u21bb"
                color: "white"
                font.pointSize: 14
            }
        }

        LoadingIndicator {
            fullscreen: true
            message: qsTr("正在加载公告...")
            isLoading: announcementVM.isLoading && announcementVM.announcements.length === 0
        }

        ListView {
            id: announcementListView
            anchors.fill: parent
            anchors.margins: Theme.spacingSmall
            spacing: Theme.spacingSmall
            clip: true
            // 禁止越界拖拽（与消息页口径一致）；列表常驻可见（空态在 footer 内容流中）
            boundsBehavior: Flickable.StopAtBounds

            model: announcementVM.announcements

            delegate: AnnouncementCard {
                width: announcementListView.width
                parentFlickable: announcementListView
                swipeManager: noticeSwipeState
                announcementId: modelData.id
                annTitle: modelData.title || ""
                annContent: modelData.content || ""
                annTime: modelData.createdAt || ""
                // 左滑显露的删除按钮点击后到此处（基类信号无参；公告为会话级本地隐藏）
                onDeleteRequested: announcementVM.hideAnnouncement(modelData.id)
                onClicked: function(data) {
                    systemNoticePage.showDetailRequest({
                        id: data.id,
                        title: data.title,
                        content: data.content,
                        type: "system",
                        createdAt: data.createdAt
                    })
                }
            }

            // 空态提示：放进内容流（footer），随列表一起滚动（不再用居中浮层）
            footer: Item {
                width: announcementListView.width
                height: emptyHint.visible ? emptyHint.height + 48 : 0

                Column {
                    id: emptyHint
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 24
                    width: Math.min(320, announcementListView.width * 0.85)
                    spacing: Theme.spacingMedium
                    visible: announcementVM.announcements.length === 0 && !announcementVM.isLoading

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "\uD83D\uDCE2"
                        font.pointSize: 48
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("暂无公告")
                        color: Theme.textHint
                        font.pointSize: Theme.fontSizeBody
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("系统公告将在这里显示")
                        color: Theme.textHint
                        font.pointSize: Theme.fontSizeCaption
                        opacity: 0.6
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            onAtYEndChanged: {
                if (atYEnd && !announcementVM.isLoading && announcementVM.hasMore)
                    announcementVM.loadNextPage()
            }
        }
    }

    Connections {
        target: announcementVM
        function onErrorOccurred(error) {
            errorMessage = ""
            errorMessage = error
        }
    }
}
