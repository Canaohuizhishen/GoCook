import QtQuick
import QtQuick.Controls
import client

// 通用「功能占位页」（v2.23）：群聊 / 粉丝等未实现入口的落地页。
// 使用方式：push 时传入 pageTitle（如 "群聊"）。
Page {
    id: placeholderPage
    title: pageTitle

    // 注意：_stackView 由 Main.qml 的组件包装注入（勿在本页重复声明——
    // 同名双声明会使页面作用域内读取恒为 null，返回按钮将失效）
    property string pageTitle: qsTr("功能")

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
            text: placeholderPage.pageTitle
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH2
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(320, parent.width * 0.85)
        spacing: Theme.spacingMedium

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "\uD83D\uDEA7"
            font.pointSize: 48
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("「%1」功能开发中").arg(placeholderPage.pageTitle)
            color: Theme.textHint
            font.pointSize: Theme.fontSizeBody
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("敬请期待")
            color: Theme.textHint
            font.pointSize: Theme.fontSizeCaption
            opacity: 0.6
        }
    }
}
