import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

// 消息页（v2.23 水位模型；布局修订：微信式两行横条 + 随列表滚动）：
//   顶部快捷入口（群聊 / 系统通知 / 粉丝）为两行横条：图标 + 标题/预览状况 + 右侧时间/徽章，
//   与下方双分类标签栏一起放入列表头部、随列表滚动（不再置顶）；标题栏固定；
//   列表禁止越界下拉（StopAtBounds）；空态提示在内容流中（footer）随滚动；
//   条目左滑仅显露删除按钮（SwipeToDeleteItem，点击才删）。
//   预览规则：群聊"暂无未读消息"；粉丝第一条关注提示（占位演示文案）；系统通知取最新公告标题+时间。
//   分类列表：审核结果 / 互动提醒，各自独立数据源与分页；进入分类即已读（无「全部已读」按钮）。
Page {
    id: notificationPage
    title: qsTr("消息")

    signal showDetailRequest(var data)
    signal showSystemNoticeRequest()
    signal showPlaceholderRequest(string pageTitle)

    property string errorMessage: ""

    // 左滑互斥管理器：同一时刻仅一个条目处于"显露删除按钮"态（SwipeToDeleteItem 契约）
    QtObject {
        id: notificationSwipeState
        property Item currentItem: null
    }

    // 页面可见时：拉取未读汇总（角标准确）+ 重载当前分类第一页（进入分类 = 已读）+ 三横条预览
    // （loadPreview 不触发已读语义——红点只能由真正进入系统通知页熄灭）
    onVisibleChanged: {
        if (visible) {
            notifyVM.refreshUnreadSummary()
            notifyVM.refresh()
            announcementVM.loadPreview()
        }
    }

    Component.onCompleted: {
        if (visible) {
            notifyVM.refreshUnreadSummary()
            notifyVM.refresh()
            announcementVM.loadPreview()
        }
    }

    // ========== 顶部标题栏（返回按钮 + 标题；固定不滚动） ==========
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
            text: qsTr("消息")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH2
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }
    }

    // 错误/成功提示（底部 toast 风格，1.5 秒自动消失）
    ErrorBanner {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 48
        anchors.horizontalCenter: parent.horizontalCenter
        text: errorMessage
    }

    // ========== 滚动内容区（快捷入口 + 分类标签 + 列表，一起滚动） ==========
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 48
        color: Theme.backgroundColor

        // 下拉刷新指示器
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 8
            width: 24; height: 24
            radius: 12
            color: Theme.primaryColor
            visible: notifyVM.isRefreshing
            z: 30

            NumberAnimation on rotation {
                from: 0; to: 360
                duration: 800
                loops: Animation.Infinite
                running: notifyVM.isRefreshing
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
            z: 20
            message: qsTr("正在加载通知...")
            isLoading: notifyVM.isLoading && notifyVM.notifications.length === 0
        }

        ListView {
            id: notificationListView
            anchors.fill: parent
            anchors.margins: Theme.spacingSmall
            clip: true
            // 禁止越界拖拽：下拉不再把头部（快捷入口 + 标签）整体拖下去
            boundsBehavior: Flickable.StopAtBounds
            // 卡片间留白（与系统通知页口径一致）；header↔首条不受此处影响
            spacing: Theme.spacingSmall
            // 注：不放 visible 绑定——列表为空时头部（快捷入口 + 标签）仍需可见
            model: notifyVM.notifications

            // ===== 列表头部：顶部快捷入口（微信式两行横条）+ 双分类标签（随列表滚动） =====
            // 注：ListView.spacing 不作用于 header 与首个 delegate 的边界，标签栏下方的间距
            // 由 bottomPadding 承担（spacing 仅作用于 header 内部：快捷卡↔标签栏 8px）
            header: Column {
                width: notificationListView.width
                spacing: Theme.spacingSmall
                bottomPadding: Theme.spacingSmall

                // ---- 顶部快捷入口：两行横条（图标 + 标题/预览 + 右侧时间/徽章） ----
                Rectangle {
                    width: parent.width
                    height: quickRows.height
                    radius: Theme.radiusMedium
                    color: Theme.cardBackground
                    border.width: 1
                    border.color: Theme.dividerColor

                    Column {
                        id: quickRows
                        width: parent.width

                        // 群聊（占位功能：徽章恒 0，保留结构将来接线不动布局；无数据源，预览固定文案）
                        Item {
                            width: parent.width
                            height: 64

                            Rectangle {
                                id: groupIconBox
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.spacingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                width: 40; height: 40
                                radius: 10
                                color: Qt.rgba(0.26, 0.52, 0.96, 0.12)

                                Text {
                                    anchors.centerIn: parent
                                    text: "\uD83D\uDC65"
                                    font.pointSize: 18
                                }
                            }

                            Item {
                                id: groupRightCol
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.spacingMedium
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 56

                                Text {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.topMargin: 13
                                    text: ""   // 群聊无数据源，时间留空
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeSmall
                                    color: Theme.textHint
                                }

                                Rectangle {
                                    property int unread: 0   // 占位：群聊功能未实现，恒 0（隐藏）
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 15
                                    width: 16; height: 16
                                    radius: 8
                                    color: "#E74C3C"
                                    visible: unread > 0

                                    Text {
                                        anchors.centerIn: parent
                                        text: parent.unread > 99 ? "99+" : parent.unread.toString()
                                        color: "white"
                                        font.pointSize: 8
                                        font.weight: Font.Bold
                                    }
                                }
                            }

                            Column {
                                anchors.left: groupIconBox.right
                                anchors.leftMargin: Theme.spacingSmall + 2
                                anchors.right: groupRightCol.left
                                anchors.rightMargin: Theme.spacingSmall
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3

                                Text {
                                    text: qsTr("群聊")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeH3
                                    color: Theme.textPrimary
                                }
                                Text {
                                    width: parent.width
                                    text: qsTr("暂无未读消息")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.spacingMedium
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.spacingMedium
                                height: 1
                                color: Theme.dividerColor
                                opacity: 0.6
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: notificationPage.showPlaceholderRequest(qsTr("群聊"))
                            }
                        }

                        // 系统通知（预览 = 最新公告标题 + 相对时间；红点 = 是否有新公告）
                        Item {
                            width: parent.width
                            height: 64

                            Rectangle {
                                id: systemIconBox
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.spacingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                width: 40; height: 40
                                radius: 10
                                color: Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.12)

                                Text {
                                    anchors.centerIn: parent
                                    text: "\uD83D\uDCE2"
                                    font.pointSize: 18
                                }
                            }

                            Item {
                                id: systemRightCol
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.spacingMedium
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 56

                                Text {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.topMargin: 13
                                    text: Theme.formatRelativeTime(announcementVM.previewCreatedAt)
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeSmall
                                    color: Theme.textHint
                                }

                                // 新内容红点（系统通知不适用未读计数，只有"有/无新内容"）
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 15
                                    width: 10; height: 10
                                    radius: 5
                                    color: "#E74C3C"
                                    visible: notifyVM.systemHasNew
                                }
                            }

                            Column {
                                anchors.left: systemIconBox.right
                                anchors.leftMargin: Theme.spacingSmall + 2
                                anchors.right: systemRightCol.left
                                anchors.rightMargin: Theme.spacingSmall
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3

                                Text {
                                    text: qsTr("系统通知")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeH3
                                    color: Theme.textPrimary
                                }
                                Text {
                                    width: parent.width
                                    text: announcementVM.previewTitle.length > 0
                                          ? announcementVM.previewTitle
                                          : qsTr("暂无公告")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.spacingMedium
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.spacingMedium
                                height: 1
                                color: Theme.dividerColor
                                opacity: 0.6
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: notificationPage.showSystemNoticeRequest()
                            }
                        }

                        // 粉丝（占位功能：徽章恒 0，保留结构将来接线不动布局；无数据源，预览为占位演示文案）
                        Item {
                            width: parent.width
                            height: 64

                            Rectangle {
                                id: fansIconBox
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.spacingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                width: 40; height: 40
                                radius: 10
                                color: Qt.rgba(0.95, 0.61, 0.07, 0.14)

                                Text {
                                    anchors.centerIn: parent
                                    text: "\u2B50"
                                    font.pointSize: 18
                                }
                            }

                            Item {
                                id: fansRightCol
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.spacingMedium
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 56

                                Text {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.topMargin: 13
                                    text: ""   // 粉丝无数据源，时间留空
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeSmall
                                    color: Theme.textHint
                                }

                                Rectangle {
                                    property int unread: 0   // 占位：粉丝功能未实现，恒 0（隐藏）
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 15
                                    width: 16; height: 16
                                    radius: 8
                                    color: "#E74C3C"
                                    visible: unread > 0

                                    Text {
                                        anchors.centerIn: parent
                                        text: parent.unread > 99 ? "99+" : parent.unread.toString()
                                        color: "white"
                                        font.pointSize: 8
                                        font.weight: Font.Bold
                                    }
                                }
                            }

                            Column {
                                anchors.left: fansIconBox.right
                                anchors.leftMargin: Theme.spacingSmall + 2
                                anchors.right: fansRightCol.left
                                anchors.rightMargin: Theme.spacingSmall
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3

                                Text {
                                    text: qsTr("粉丝")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeH3
                                    color: Theme.textPrimary
                                }
                                Text {
                                    width: parent.width
                                    text: qsTr("美食家小林 关注了你")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: notificationPage.showPlaceholderRequest(qsTr("粉丝"))
                            }
                        }
                    }
                }

                // ---- 分类标签栏（审核结果 / 互动提醒，各自独立数据源） ----
                Rectangle {
                    width: parent.width
                    height: 44
                    radius: Theme.radiusMedium
                    color: Theme.cardBackground
                    border.color: Theme.dividerColor
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingSmall
                        anchors.rightMargin: Theme.spacingSmall
                        spacing: Theme.spacingXSmall

                        Button {
                            text: notifyVM.reviewUnread > 0
                                  ? qsTr("审核结果") + " (" + notifyVM.reviewUnread + ")"
                                  : qsTr("审核结果")
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
                            text: notifyVM.interactionUnread > 0
                                  ? qsTr("互动提醒") + " (" + notifyVM.interactionUnread + ")"
                                  : qsTr("互动提醒")
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
            }

            delegate: NotificationCardDelegate {
                width: notificationListView.width
                parentFlickable: notificationListView
                swipeManager: notificationSwipeState
                notificationId: modelData.id
                notifTitle: modelData.title || ""
                notifContent: modelData.content || ""
                createdAt: modelData.createdAt || ""
                notifType: modelData.type || ""
                isRead: modelData.is_read || false
                // 左滑显露的删除按钮点击后到此处（基类信号无参；用 modelData.id 定位条目）
                onDeleteRequested: notifyVM.deleteNotification(modelData.id)
                onClicked: function(data) { notificationPage.showDetailRequest(data) }
                // 注：无 onReadRequested —— 进入分类即已读，不再有单条标读
            }
            // 空态提示：放进内容流（footer），随列表一起滚动——不再用浮层（避免滚动时遮盖头部）
            footer: Item {
                width: notificationListView.width
                height: emptyHint.visible ? emptyHint.height + 48 : 0

                Column {
                    id: emptyHint
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 24
                    width: Math.min(320, notificationListView.width * 0.85)
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
                        text: qsTr("当有审核结果或互动提醒时，将在这里显示")
                        color: Theme.textHint
                        font.pointSize: Theme.fontSizeCaption
                        opacity: 0.6
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        width: parent.width/1.5
                    }
                }
            }

            // 触底加载更多
            onAtYEndChanged: {
                if (atYEnd && !notifyVM.isLoading && notifyVM.hasMore)
                    notifyVM.loadNextPage()
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
        function onDeleteSuccess(id) {
            // 删除成功，列表已自动更新
        }
    }
}
