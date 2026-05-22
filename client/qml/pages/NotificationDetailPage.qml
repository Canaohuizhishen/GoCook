import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: detailPage
    title: qsTr("通知详情")

    // 从 NotificationPage 传入的通知数据
    property var notificationData: ({})

    // ========== 顶部标题栏（返回 + 标题） ==========
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
            text: qsTr("通知详情")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH2
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }
    }

    // ========== 内容区域（可滚动） ==========
    ScrollView {
        anchors.top: parent.top
        anchors.topMargin: 48
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingMedium
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingMedium

            // 类型标签 + 时间
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                // 类型标签
                Rectangle {
                    width: typeLabel.implicitWidth + 16
                    height: 24
                    radius: 12
                    color: {
                        if (notificationData.type === "system") return Theme.primaryColor
                        if (notificationData.type === "review") return "#F39C12"
                        if (notificationData.type === "interaction") return "#2ECC71"
                        return Theme.textHint
                    }

                    Text {
                        id: typeLabel
                        anchors.centerIn: parent
                        text: {
                            if (notificationData.type === "system") return qsTr("系统公告")
                            if (notificationData.type === "review") return qsTr("审核结果")
                            if (notificationData.type === "interaction") return qsTr("互动提醒")
                            return notificationData.type || ""
                        }
                        color: "white"
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        font.weight: Font.Bold
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: {
                        var date = notificationData.createdAt || ""
                        if (date.length > 10)
                            return date.substring(0, 10) + " " + date.substring(11, 19)
                        return date
                    }
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeSmall
                    color: Theme.textHint
                }
            }

            // 标题
            Text {
                text: notificationData.title || ""
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH2
                font.weight: Font.Bold
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // 分隔线
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.dividerColor
            }

            // 完整内容
            Text {
                text: notificationData.content || ""
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                lineHeight: 1.6
                Layout.fillWidth: true
            }

            // 触发人（如果有）
            Text {
                text: notificationData.triggerUserName ? qsTr("触发者：%1").arg(notificationData.triggerUserName) : ""
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textHint
                visible: text.length > 0
                Layout.topMargin: Theme.spacingLarge
            }
        }
    }
}
