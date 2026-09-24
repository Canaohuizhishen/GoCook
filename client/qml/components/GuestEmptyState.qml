import QtQuick
import QtQuick.Controls
import client

// 登录空态（游客通用）：图标 + 主/副文案 + 「去登录」按钮。
// 供"入口总是可见、登录态只决定进去后能不能用"的个人数据类页面复用
// （我的投稿 / 我的评论 / 消息页 / 收藏页）。
// 布局契约：本组件为内容自适应 Column，不自行锚定——宿主用 anchors.centerIn（居中空态）
// 或放进内容流；点击按钮发 actionRequested()，由宿主信号上抛、Main.qml 动作守卫（guardAction）
// 弹应用内登录页，登录成功后由宿主页面自行刷新（原地刷新、不 pop 回上层）。
Column {
    id: root

    property string iconText: ""                 // 图标（emoji 文本；空串隐藏）
    property string title: ""                    // 主文案（如「登录后查看你的投稿」）
    property string subtitle: ""                 // 副文案（一句话说明登录后能做什么；空串隐藏）
    property string actionText: qsTr("去登录")   // 按钮文案

    signal actionRequested()                     // 点击「去登录」（宿主接住后走动作守卫）

    spacing: Theme.spacingMedium

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.iconText
        font.pointSize: 48
        visible: root.iconText.length > 0
    }

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.title
        color: Theme.textHint
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeBody
    }

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: root.subtitle.length > 0
        text: root.subtitle
        color: Theme.textHint
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeCaption
        opacity: 0.6
    }

    CustomButton {
        anchors.horizontalCenter: parent.horizontalCenter
        buttonText: root.actionText
        buttonType: CustomButton.ButtonType.Primary
        onClicked: root.actionRequested()
    }
}
