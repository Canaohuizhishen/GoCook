import QtQuick
import QtQuick.Controls
import "../styles"

Rectangle {
    id: loadingIndicator

    // 属性配置
    property bool isLoading: false
    property alias message: messageText.text
    property bool fullscreen: false    // 全屏遮罩模式
    property color overlayColor: "#80000000"

    visible: isLoading
    color: fullscreen ? overlayColor : "transparent"
    anchors.fill: parent

    // 阻止鼠标事件穿透
    MouseArea {
        anchors.fill: parent
        enabled: loadingIndicator.fullscreen
        hoverEnabled: false
    }

    // 加载内容卡片
    Rectangle {
        anchors.centerIn: parent
        width: fullscreen ? 120 : Math.min(parent.width * 0.6, 200)
        height: fullscreen ? 120 : 80
        radius: Theme.radiusLarge
        color: fullscreen ? Theme.cardBackground : "transparent"
        opacity: fullscreen ? 0.95 : 1.0

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationMedium }
        }

        Column {
            anchors.centerIn: parent
            spacing: Theme.spacingSmall

            // 旋转的加载动画
            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: loadingIndicator.isLoading
                width: 40
                height: 40
                palette.dark: Theme.primaryColor
            }

            // 提示文字（可选）
            Text {
                id: messageText
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("加载中...")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: fullscreen ? Theme.textSecondary : Theme.textPrimary
                visible: text !== ""
            }
        }
    }

    // 状态变化动画
    Behavior on opacity {
        NumberAnimation { duration: Theme.durationMedium }
    }
}