import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    title: qsTr("个人中心")

    signal showSubmitRequest()
    signal showMyRecipesRequest()
    signal showMyRatingsRequest()
    signal showSettingsRequest()
    signal showNotificationRequest()

    // 页面创建时和每次可见时都加载最新用户资料 + 通知未读数
    Component.onCompleted: {
        authViewModel.loadProfile()
        notifyVM.loadNotifications(1, 20)
    }
    onVisibleChanged: {
        if (visible) {
            authViewModel.loadProfile()
            notifyVM.loadNotifications(1, 20)
        }
    }
    // StackView 中从子页面返回时 onVisibleChanged 未必触发，
    // 用 onActivated 保证每次回到本页都刷新
    StackView.onActivated: {
        authViewModel.loadProfile()
        notifyVM.loadNotifications(1, 20)
    }

    // ========== 居中容器（全宽自适应） ==========
    Item {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingMedium

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingSmall

            // ★ 圆形头像（ShaderEffect 遮罩，全 Qt 版本兼容）
            CircularImage {
                id: profileAvatar
                Layout.alignment: Qt.AlignHCenter
                width: 72; height: 72
                borderColor: Theme.dividerColor
                borderWidth: 1.5
                placeholderFallback: {
                    var n = authViewModel.profileDisplayName
                    if (n.length > 0) return n.charAt(0).toUpperCase()
                    n = authViewModel.username
                    if (n.length > 0) return n.charAt(0).toUpperCase()
                    return "G"
                }
                placeholderText.font.family: Theme.fontFamily
                placeholderText.font.weight: Theme.fontWeightMedium
                placeholderText.color: Theme.textHint

                // 直接绑定 source，比 Connections 命令式赋值更稳定
                source: {
                    var url = authViewModel.profileAvatarUrl
                    return url.length > 0 ? authViewModel.apiBaseUrl + url : ""
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: authViewModel.loggedIn ? qsTr("欢迎, %1").arg(authViewModel.username) : qsTr("未登录")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH2
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: authViewModel.loggedIn ? qsTr("用户 ID: %1").arg(authViewModel.userId) : ""
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textHint
                visible: authViewModel.loggedIn
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.dividerColor
        }

        // ========== 设置（Canvas 齿轮图标） ==========
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            visible: authViewModel.loggedIn
            radius: Theme.radiusMedium
            color: settingsBtn.containsMouse ? Qt.rgba(0,0,0,0.05) : "transparent"
            border.color: Theme.primaryColor
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                Canvas {
                    id: gearCanvas
                    implicitWidth: 16
                    implicitHeight: 16
                    property color iconColor: Theme.primaryColor
                    onIconColorChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.save();
                        ctx.translate(2, 2);
                        ctx.scale(12/18, 12/18);
                        ctx.strokeStyle = iconColor;
                        ctx.lineWidth = 1.8;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";

                        var cx = 9, cy = 9;

                        // 齿轮外圈
                        ctx.beginPath();
                        ctx.arc(cx, cy, 5.5, 0, Math.PI * 2);
                        ctx.stroke();

                        // 6 个齿
                        for (var i = 0; i < 6; i++) {
                            var angle = i * Math.PI / 3 - Math.PI / 6;
                            ctx.beginPath();
                            ctx.moveTo(cx + 5 * Math.cos(angle), cy + 5 * Math.sin(angle));
                            ctx.lineTo(cx + 7.5 * Math.cos(angle), cy + 7.5 * Math.sin(angle));
                            ctx.stroke();
                        }

                        // 中心孔
                        ctx.beginPath();
                        ctx.arc(cx, cy, 1.8, 0, Math.PI * 2);
                        ctx.stroke();

                        ctx.restore();
                    }
                }

                Text {
                    text: qsTr("设置")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.primaryColor
                }
            }

            MouseArea {
                id: settingsBtn
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: showSettingsRequest()
            }
        }

        // ========== 发布菜谱（Canvas 钢笔图标） ==========
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            visible: authViewModel.loggedIn
            radius: Theme.radiusMedium
            color: publishBtn.containsMouse ? Qt.rgba(0,0,0,0.05) : "transparent"
            border.color: Theme.primaryColor
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                Canvas {
                    id: penCanvas
                    implicitWidth: 16
                    implicitHeight: 16
                    property color iconColor: Theme.primaryColor
                    onIconColorChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.save();
                        ctx.strokeStyle = iconColor;
                        ctx.lineWidth = 1.8;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";

                        // 笔身（倾斜）
                        ctx.beginPath();
                        ctx.moveTo(13, 1);
                        ctx.lineTo(6, 8);
                        ctx.stroke();

                        // 笔尖左侧
                        ctx.beginPath();
                        ctx.moveTo(6, 8);
                        ctx.lineTo(2, 14);
                        ctx.stroke();

                        // 笔尖右侧
                        ctx.beginPath();
                        ctx.moveTo(6, 8);
                        ctx.lineTo(10, 14);
                        ctx.stroke();

                        // 笔帽装饰线
                        ctx.beginPath();
                        ctx.moveTo(11, 3);
                        ctx.lineTo(13, 1);
                        ctx.stroke();

                        ctx.restore();
                    }
                }

                Text {
                    text: qsTr("发布菜谱")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.primaryColor
                }
            }

            MouseArea {
                id: publishBtn
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: showSubmitRequest()
            }
        }

        // ========== 我的投稿（内联 SVG 文档图标） ==========
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            visible: authViewModel.loggedIn
            radius: Theme.radiusMedium
            color: myRecipesBtn.containsMouse ? Qt.rgba(0,0,0,0.05) : "transparent"
            border.color: Theme.primaryColor
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                Canvas {
                    id: docCanvas
                    implicitWidth: 16
                    implicitHeight: 16
                    property color iconColor: Theme.primaryColor
                    onIconColorChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.save();
                        ctx.translate(2, 2);
                        ctx.scale(12/18, 12/18);
                        ctx.strokeStyle = iconColor;
                        ctx.lineWidth = 1.8;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";

                        // 文件主体 (矩形, 左上角开口供折角)
                        ctx.beginPath();
                        ctx.moveTo(5, 2);       // 左上角折角起点
                        ctx.lineTo(15, 2);       // 上边右
                        ctx.lineTo(17, 2);
                        ctx.arcTo(18, 2, 18, 4, 2);  // 右上圆角
                        ctx.lineTo(18, 16);      // 右边下
                        ctx.arcTo(18, 18, 16, 18, 2); // 右下圆角
                        ctx.lineTo(4, 18);       // 下边左
                        ctx.arcTo(2, 18, 2, 16, 2);  // 左下圆角
                        ctx.lineTo(2, 4);        // 左边上
                        ctx.arcTo(2, 2, 4, 2, 2);    // 左上圆角
                        ctx.lineTo(5, 2);        // 回到折角起点
                        ctx.stroke();

                        // 折角线 (右上小三角)
                        ctx.beginPath();
                        ctx.moveTo(5, 2);
                        ctx.lineTo(5, 7);
                        ctx.lineTo(12, 7);
                        ctx.lineTo(12, 2);
                        ctx.stroke();

                        // 文字行 1
                        ctx.beginPath();
                        ctx.moveTo(6, 11);
                        ctx.lineTo(15, 11);
                        ctx.stroke();

                        // 文字行 2
                        ctx.beginPath();
                        ctx.moveTo(6, 14);
                        ctx.lineTo(13, 14);
                        ctx.stroke();
                        ctx.restore();
                    }
                }

                Text {
                    text: qsTr("我的投稿")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.primaryColor
                    verticalAlignment: Text.AlignVCenter
                }
            }

            MouseArea {
                id: myRecipesBtn
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: showMyRecipesRequest()
            }
        }

        // ========== 我的评论（Canvas 消息气泡图标） ==========
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            visible: authViewModel.loggedIn
            radius: Theme.radiusMedium
            color: myRatingsBtn.containsMouse ? Qt.rgba(0,0,0,0.05) : "transparent"
            border.color: Theme.primaryColor
            border.width: 1

            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                Canvas {
                    id: bubbleCanvas
                    implicitWidth: 16
                    implicitHeight: 16
                    property color iconColor: Theme.primaryColor
                    onIconColorChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.save();
                        ctx.translate(2, 2);
                        ctx.scale(12/18, 12/18);
                        ctx.strokeStyle = iconColor;
                        ctx.lineWidth = 1.8;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";

                        // 消息气泡：圆角矩形 + 左下角尾巴
                        ctx.beginPath();
                        ctx.moveTo(5, 2);
                        ctx.lineTo(15, 2);
                        ctx.arcTo(18, 2, 18, 5, 3);   // 右上圆角
                        ctx.lineTo(18, 13);
                        ctx.arcTo(18, 16, 15, 16, 3); // 右下圆角
                        ctx.lineTo(8, 16);             // 尾巴底部
                        ctx.lineTo(6, 18);             // 尾巴尖
                        ctx.lineTo(4, 16);             // 尾巴顶部
                        ctx.lineTo(3, 16);
                        ctx.arcTo(0, 16, 0, 13, 3);   // 左下圆角
                        ctx.lineTo(0, 5);
                        ctx.arcTo(0, 2, 3, 2, 3);     // 左上圆角
                        ctx.closePath();
                        ctx.stroke();
                        ctx.restore();
                    }
                }

                Text {
                    text: qsTr("我的评论")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.primaryColor
                    verticalAlignment: Text.AlignVCenter
                }
            }

            MouseArea {
                id: myRatingsBtn
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: showMyRatingsRequest()
            }
        }

        Item { Layout.fillHeight: true }

        CustomButton {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingMedium
            buttonText: qsTr("退出登录")
            buttonType: CustomButton.ButtonType.Secondary
            visible: authViewModel.loggedIn
            onClicked: authViewModel.logout()
        }
    }

    // ========== 消息通知图标（右上角） ==========
    Item {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: -Theme.spacingSmall
        anchors.rightMargin: -Theme.spacingSmall
        width: 44; height: 44
        visible: authViewModel.loggedIn

        Button {
            anchors.fill: parent
            flat: true
            contentItem: Canvas {
                width: 24
                height: 24
                property color iconColor: Theme.textPrimary
                onIconColorChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.strokeStyle = iconColor
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(7, 8)
                    ctx.arc(12, 8, 5, Math.PI, 0, false)
                    ctx.lineTo(17, 14)
                    ctx.lineTo(19, 17)
                    ctx.lineTo(5, 17)
                    ctx.lineTo(7, 14)
                    ctx.closePath()
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(12, 17)
                    ctx.lineTo(12, 19)
                    ctx.arc(12, 20, 1.5, Math.PI, 0, false)
                    ctx.stroke()
                }
            }
            onClicked: showNotificationRequest()
        }

        // 未读红点
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 2
            anchors.rightMargin: 0
            width: 12; height: 12
            radius: 6
            color: "#E74C3C"
            visible: notifyVM.unreadCount > 0

            Text {
                anchors.centerIn: parent
                text: notifyVM.unreadCount > 99 ? "99+" : notifyVM.unreadCount.toString()
                color: "white"
                font.pointSize: 8
                font.weight: Font.Bold
                visible: notifyVM.unreadCount > 0
            }
        }
    }
}
}
