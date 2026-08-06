import QtQuick
import QtQuick.Controls
import client

// 加载失败视图（主流 App 形态）：页面内容隐藏，居中提示「服务器有点问题，请稍候再试」+ 刷新按钮。
// 使用时机：加载转圈（LoadingIndicator）→ 请求超时/重试耗尽失败 → 本视图；有缓存（或内存旧数据）时静默显示，不使用本组件。
Item {
    id: offlineView

    property bool active: false
    property string message: qsTr("服务器有点问题，请稍候再试")
    signal retryRequested()

    visible: active
    anchors.fill: parent
    // 必须在页面内容（Flickable/ListView 等）之上：它们会吞掉鼠标按压，否则刷新按钮点不到
    z: 100

    // 阻断点击穿透：视图可见时下层内容不可交互（主流 App 行为）
    MouseArea {
        anchors.fill: parent
        enabled: offlineView.active
    }

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingLarge

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: offlineView.message
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            color: Theme.textHint
            horizontalAlignment: Text.AlignHCenter
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("刷新")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
            leftPadding: 32
            rightPadding: 32
            topPadding: 8
            bottomPadding: 8

            background: Rectangle {
                radius: Theme.radiusMedium
                color: parent.down ? Qt.darker(Theme.primaryColor, 1.15) : Theme.primaryColor
                Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
            }
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: offlineView.retryRequested()
        }
    }
}
