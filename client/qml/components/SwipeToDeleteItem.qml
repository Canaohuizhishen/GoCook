import QtQuick
import QtQuick.Controls
import client

/**
 * SwipeToDeleteItem — iOS 备忘录风格左滑删除列表项组件
 * 交互：左滑显露删除按钮（点击才删）；删除时收起左滑状态、延时发 deleteRequested，
 * 条目由使用方数据源移除（不做高度收缩动画）；同一时刻仅一项展开（swipeManager 互斥）。
 */
Item {
    id: root

    // ============================================================
    // 公开属性
    // ============================================================

    default property alias contentData: contentContainer.data
    property alias contentItem: contentContainer

    property real buttonWidth: 80
    property real threshold: buttonWidth / 2
    property bool swipeEnabled: true
    property Flickable parentFlickable: null
    property QtObject swipeManager: null

    readonly property bool expanded: _expanded
    readonly property bool deleting: _deleting

    implicitHeight: 72

    // ============================================================
    // 信号
    // ============================================================

    signal deleteRequested()
    signal contentClicked()
    signal expandStarted()
    signal expandEnded()

    // ============================================================
    // 内部状态
    // ============================================================

    property bool _expanded: false
    property bool _deleting: false

    property real _startContentX: 0
    property bool _horizontalDrag: false
    property bool _dragConsumed: false
    property int _gestureId: 0

    // ============================================================
    // 删除流程：无收缩动画（收起左滑 → 条目由使用方数据源移除）
    // ============================================================

    clip: true

    // 底层：给整个组件铺一层卡片底色。
    // 防止 contentArea 圆角处漏出父级背景（白线/浅色线）。
    // z:-100 确保压在所有子元素之下。
    Rectangle {
        anchors.fill: parent
        color: Theme.cardBackground
        radius: Theme.radiusMedium + 2
        z: -100
    }

    Rectangle {
        id: maskLayer
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
        }
        radius: Theme.radiusMedium
        width: root.width / 2
        color: Theme.errorColor
        clip: true
        visible: contentArea.x < -1
    }

    // ============================================================
    // 底层：删除按钮
    // ============================================================

    Rectangle {
        id: deleteBtn

        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
        width: root.buttonWidth
        radius: Theme.radiusMedium + 4

        visible: root.swipeEnabled && !root._deleting && contentArea.x < -1
        color: Theme.errorColor

        Text {
            anchors.centerIn: parent
            text: "🗑"
            font.pixelSize: 22
            color: "#FFFFFF"
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (!root._deleting) {
                    // 删除流程：先收起左滑状态，延时发信号；条目由使用方数据源移除（无收缩动画）
                    root._deleting = true
                    root.collapse()
                    deleteFinishTimer.restart()
                }
            }
        }
    }

    Timer {
        id: deleteFinishTimer
        interval: 280   // 与左滑收起动画（弹簧回位）时长对齐；回调后由数据源移除条目
        onTriggered: root.deleteRequested()
    }

    // ============================================================
    // 上层：可滑动内容区域
    // ============================================================

    Rectangle {
        id: contentArea

        anchors {
            top: parent.top
            bottom: parent.bottom
            // 底边向下超出 2px，被 root.clip 裁掉，
            // 消除圆角弧线底部的抗锯齿缝（1px 露背景）
            bottomMargin: -2
        }
        width: root.width
        x: 0

        radius: Theme.radiusMedium + 2
        color: Theme.cardBackground
        clip: true

        Behavior on x {
            enabled: !root._horizontalDrag
            SpringAnimation {
                spring: 5.0
                damping: 0.35
                epsilon: 0.001
            }
        }

        Item {
            id: contentContainer
            anchors.fill: parent
            anchors.leftMargin: 20
        }

        // 底部分割线 —— 若这条线本身颜色偏白，也会被看成"白线"
        // 可将其改为更贴近卡片底色的半透明色，例如 "#14000000"
        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 0.5
            color: Theme.dividerColor
        }

        // ============================================================
        // 手势识别
        // ============================================================

        MouseArea {
            id: gestureArea
            anchors.fill: parent

            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.ArrowCursor
            hoverEnabled: false
            preventStealing: true

            property point _startScenePos: Qt.point(0, 0)

            // ---- 按压 ----
            onPressed: function(mouse) {
                if (!root.swipeEnabled || root._deleting) return

                root._gestureId++

                root._startContentX = contentArea.x
                root._horizontalDrag = false
                root._dragConsumed = false

                var sp = mapToItem(null, mouse.x, mouse.y)
                _startScenePos = Qt.point(sp.x, sp.y)

                if (root.swipeManager && root.swipeManager.currentItem
                        && root.swipeManager.currentItem !== root) {
                    root.swipeManager.currentItem.collapse()
                }
            }

            // ---- 移动 ----
            onPositionChanged: function(mouse) {
                if (!root.swipeEnabled || root._deleting || root._dragConsumed) return
                if (!pressed) return

                var sp = mapToItem(null, mouse.x, mouse.y)
                var dx = sp.x - _startScenePos.x
                var dy = sp.y - _startScenePos.y

                if (!root._horizontalDrag) {
                    var absDx = Math.abs(dx)
                    var absDy = Math.abs(dy)

                    if (absDx < 3 && absDy < 3) return

                    if (absDx > absDy) {
                        root._horizontalDrag = true
                        mouse.accepted = true

                        if (root.parentFlickable) {
                            root.parentFlickable.interactive = false
                        }
                        if (root.swipeManager) {
                            root.swipeManager.currentItem = root
                        }
                        root.expandStarted()
                    } else {
                        mouse.accepted = false
                        return
                    }
                }

                if (root._horizontalDrag) {
                    mouse.accepted = true

                    var newX = root._startContentX + dx

                    if (newX > 15) newX = 15
                    if (newX < -root.buttonWidth - 20) newX = -root.buttonWidth - 20

                    contentArea.x = newX
                }
            }

            // ---- 松开 ----
            onReleased: function(mouse) {
                if (!root.swipeEnabled || root._deleting) return

                if (root._horizontalDrag) {
                    mouse.accepted = true

                    var currentX = contentArea.x
                    root._horizontalDrag = false

                    if (!root._expanded) {
                        if (currentX < -root.threshold) {
                            root._expanded = true
                            contentArea.x = -root.buttonWidth
                        } else {
                            root._expanded = false
                            contentArea.x = 0
                        }
                    } else {
                        if (currentX > -(root.buttonWidth - root.threshold)) {
                            root._expanded = false
                            contentArea.x = 0
                            root.expandEnded()
                        } else {
                            root._expanded = true
                            contentArea.x = -root.buttonWidth
                        }
                    }

                    if (root.parentFlickable) {
                        root.parentFlickable.interactive = true
                    }

                    root._dragConsumed = true
                    return
                }

                root._horizontalDrag = false
            }

            // ---- 点击 ----
            onClicked: function(mouse) {
                if (root._dragConsumed) {
                    root._dragConsumed = false
                    return
                }

                if (!root.swipeEnabled || root._deleting) return

                if (root._expanded) {
                    root.collapse()
                    mouse.accepted = true
                } else {
                    root.contentClicked()
                    mouse.accepted = false
                }
            }
        }
    }

    // ============================================================
    // 互斥监听
    // ============================================================

    property QtObject _connectedManager: null

    onSwipeManagerChanged: {
        if (_connectedManager) {
            try { _connectedManager.currentItemChanged.disconnect(_onManagerChanged) } catch(e) {}
        }
        _connectedManager = root.swipeManager
        if (root.swipeManager) {
            root.swipeManager.currentItemChanged.connect(_onManagerChanged)
        }
    }

    Component.onDestruction: {
        if (_connectedManager) {
            try { _connectedManager.currentItemChanged.disconnect(_onManagerChanged) } catch(e) {}
            _connectedManager = null
        }
    }

    function _onManagerChanged() {
        if (!_connectedManager) return
        if (_connectedManager.currentItem !== root && root._expanded) {
            root.collapse()
        }
    }

    // ============================================================
    // 公开方法
    // ============================================================

    function expand() {
        if (!root.swipeEnabled || root._deleting) return

        if (root.swipeManager && root.swipeManager.currentItem
                && root.swipeManager.currentItem !== root) {
            root.swipeManager.currentItem.collapse()
        }
        if (root.swipeManager) {
            root.swipeManager.currentItem = root
        }

        root._expanded = true
        contentArea.x = -root.buttonWidth
        root.expandStarted()
    }

    function collapse() {
        root._expanded = false
        contentArea.x = 0
        root.expandEnded()

        if (root.swipeManager && root.swipeManager.currentItem === root) {
            root.swipeManager.currentItem = null
        }
    }
}