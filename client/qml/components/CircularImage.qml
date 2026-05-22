import QtQuick

/**
 * CircularImage — 圆形图片组件
 *
 * v1: 最简版本——直接显示 Image，叠加圆形边框装饰。
 * 圆形裁剪留待后续用 QtQuick.Shapes 或 QPainter 路径单独实现。
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

    property string placeholderFallback: "?"
    property alias  placeholderText:     placeholder

    implicitWidth:  110
    implicitHeight: 110

    // ============================================================
    // 图片（直接可见）
    // ============================================================
    Image {
        id: img
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }

    // ============================================================
    // 占位文字
    // ============================================================
    Text {
        id: placeholder
        anchors.centerIn: parent
        text: root.placeholderFallback
        font.pointSize: Math.min(root.width, root.height) * 0.45
        color: "#888"
        visible: img.status === Image.Null || img.status === Image.Error
    }

    // ============================================================
    // 圆形边框装饰
    // ============================================================
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.color: root.borderColor
        border.width: root.borderWidth
        antialiasing: true
    }
}
