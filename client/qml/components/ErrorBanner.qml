import QtQuick
import QtQuick.Controls
import client
import "."

// 底部 toast 风格错误提示（主流 App 形态）：黑色半透明圆角胶囊 + 白色文字，淡入淡出后自动消失。
// 定位由使用方指定（建议 anchors.bottom + anchors.horizontalCenter 居中）。
// 绑定说明：只置 autoDismissed 标志、不写 text——命令式清空会永久打破外部 text 绑定（text: errorMessage），
// 导致同页面后续错误不再显示；调用方约定：同文案连续错误时先清空再赋值（errorMessage = ""; errorMessage = error），
// 否则值未变不触发 onTextChanged，自动消失后不会再次出现。
Rectangle {
    id: root
    height: 36
    radius: height / 2
    color: "#BF000000"
    z: 100

    property string text: ""
    property int autoDismissMs: 1500
    property bool autoDismissed: false

    signal dismissed()

    // 胶囊宽度随文字自适应（上限 480，避免超长文案撑满全屏）
    width: Math.min(toastText.width + 48, 480)

    opacity: (text.length > 0 && !autoDismissed) ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.durationMedium } }

    Timer {
        id: timer
        interval: root.autoDismissMs
        onTriggered: {
            root.autoDismissed = true
            root.dismissed()
        }
    }

    onTextChanged: {
        if (text.length > 0) {
            autoDismissed = false
            timer.restart()
        }
    }

    Text {
        id: toastText
        anchors.centerIn: parent
        text: root.text
        color: "white"
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeBody
        horizontalAlignment: Text.AlignHCenter
    }
}
