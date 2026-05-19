import QtQuick

/**
 * CircularImage — 圆形图片组件
 *
 * 使用 Rectangle.clip + radius 做圆形裁剪，
 * 兼容 Qt 6.0–6.11 全版本。
 *
 * 用法:
 *   CircularImage {
 *       width: 110; height: 110
 *       source: "http://..."
 *       borderColor: Theme.dividerColor
 *       borderWidth: 1.5
 *   }
 */
Item {
    id: root

    // -- 公开属性 --
    property alias source:              img.source
    property alias fillMode:            img.fillMode
    property alias status:              img.status
    property alias asynchronous:        img.asynchronous
    property alias progress:            img.progress
    property alias sourceSize:          img.sourceSize

    property color borderColor: "transparent"
    property real  borderWidth: 0

    // 无图片时显示的首字母
    property string placeholderFallback: "?"
    property alias  placeholderText:     placeholder

    // -- 内部布局 --
    implicitWidth:  110
    implicitHeight: 110

    // 裁剪容器 —— 用 radius 做圆形裁剪
    Rectangle {
        id: clipMask
        anchors.fill: parent
        radius: Math.min(width, height) / 2
        clip: true
        color: "transparent"

        Image {
            id: img
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }
    }

    // 无头像时的占位文字
    Text {
        id: placeholder
        anchors.centerIn: parent
        text: root.placeholderFallback
        font.pointSize: Math.min(root.width, root.height) * 0.45
        color: "#888"
        visible: img.status === Image.Null
    }

    // 圆形边框（纯装饰）
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.color: root.borderColor
        border.width: root.borderWidth
        antialiasing: true
    }
}
