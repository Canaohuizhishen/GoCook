import QtQuick
import QtQuick.Controls
import client

/**
 * SwipeToDeleteItem — iOS 备忘录风格左滑删除列表项组件
 *
 * 提供一个可复用的两层级滑动容器：
 *   - 底层：右侧红色删除按钮（80px 宽，圆角，白色🗑图标）
 *   - 上层：用户内容区域，跟手滑动露出按钮
 *
 * 用法示例：
 *   // 1. 在父级定义互斥管理器
 *   QtObject { id: swipeState; property Item currentItem: null }
 *
 *   // 2. 在 ListView/Repeater delegate 中使用
 *   SwipeToDeleteItem {
 *       width: parent.width
 *       height: 72
 *       parentFlickable: scrollArea          // 父级 Flickable/ListView
 *       swipeManager: swipeState             // 共享管理器
 *       onDeleteRequested: {  执行删除  }
 *
 *       // 自定义内容（default property，直接写子元素）
 *       RowLayout { ... }
 *   }
 *
 * 特性：
 *   - 1:1 跟手滑动，弹性弹簧动画（stiffness: 300, damping: 30 等效）
 *   - 40px 阈值判定自动展开/收回
 *   - 互斥：同一时刻仅一个项展开
 *   - 点击内容区域或列表外区域收回
 *   - 滑动时锁定父级垂直滚动
 *   - 同时支持触摸（移动端）与鼠标（桌面端）
 *   - 删除时伴随高度收缩动画
 */
Item {
    id: root

    // ============================================================
    // 公开属性
    // ============================================================

    /// 用户自定义内容 —— default property，直接写子元素即可
    default property alias contentData: contentContainer.data
    property alias contentItem: contentContainer

    /// 删除按钮宽度（px），默认 80
    property real buttonWidth: 80

    /// 滑动阈值（px），默认 = 按钮宽度的一半
    property real threshold: buttonWidth / 2

    /// 是否启用滑动功能（设为 false 则禁用所有滑动手势）
    property bool swipeEnabled: true

    /// 父级 Flickable 引用 —— 用于水平滑动时锁定垂直滚动
    property Flickable parentFlickable: null

    /// 互斥管理器 QtObject —— 需包含 property Item currentItem
    /// 同一时刻仅一个 SwipeToDeleteItem 可展开
    property QtObject swipeManager: null

    /// 是否处于展开状态（只读）
    readonly property bool expanded: _expanded

    /// 是否正在播放删除收缩动画（只读）
    readonly property bool deleting: _deleting

    /// 默认高度（未设置 height / implicitHeight 时的回退值）
    /// 可在使用时通过 height 或 implicitHeight 覆盖
    implicitHeight: 72

    // ============================================================
    // 信号
    // ============================================================

    /// 用户点击了删除按钮
    signal deleteRequested()

    /// 用户点击了内容区域（非拖拽、非展开态下的轻触）
    signal contentClicked()

    /// 某个项开始展开（给管理器／父级用于副作用）
    signal expandStarted()

    /// 某个项完成收回
    signal expandEnded()

    // ============================================================
    // 内部状态
    // ============================================================

    property bool _expanded: false
    property bool _deleting: false

    // 手势追踪
    property real _startContentX: 0       // 按压时 contentArea.x 的快照
    property real _startMouseX: 0         // 按压时鼠标/手指的屏幕 x
    property real _startMouseY: 0
    property bool _horizontalDrag: false  // 是否已确认为水平拖拽
    property bool _dragConsumed: false    // 防止重复消费
    property int _gestureId: 0            // 自增 ID，防止过期回调冲突

    // ============================================================
    // 删除动画 —— 高度 + 透明度收缩
    // ============================================================

    height: _deleting ? 0 : implicitHeight
    clip: true
    visible: height > 0 || !_deleting  // 收缩到 0 后隐藏，腾出布局空间

    Behavior on height {
        NumberAnimation {
            duration: 250
            easing.type: Easing.InOutQuad
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: 200
        }
    }

    // 红色遮掩层：遮盖删除按钮的左侧圆角边界，避免露出底层间隙
    // 仅在内容滑开时可见，垂直滚动时隐藏防止红色残影
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
        radius: Theme.radiusMedium+4

        // 内容完全覆盖时隐藏，避免垂直滚动露出红色残影
        visible: root.swipeEnabled && !root._deleting && contentArea.x < -1

        color: Theme.errorColor   // 项目主题红色

        // 白色垃圾桶图标
        Text {
            anchors.centerIn: parent
            text: "🗑"
            font.pixelSize: 22
            color: "#FFFFFF"
        }

        // 删除按钮点击区域
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (!root._deleting) {
                    root._deleting = true
                    // 先播放收缩动画
                    root.collapse()
                    // 动画完成后发射信号
                    deleteFinishTimer.restart()
                }
            }
        }
    }

    // 延迟发射删除信号，让收缩动画先播放
    Timer {
        id: deleteFinishTimer
        interval: 280
        onTriggered: {
            root.deleteRequested()
        }
    }

    // ============================================================
    // 上层：可滑动内容区域
    // ============================================================

    Rectangle {
        id: contentArea

        anchors {
            top: parent.top
            bottom: parent.bottom
        }
        width: root.width
        // x = 0（隐藏按钮） ~ -buttonWidth（完全露出按钮）
        x: 0

        radius: Theme.radiusMedium+2
        color: Theme.cardBackground   // 项目主题卡片背景
        clip: true

        // ---- iOS 弹簧动画 ----
        Behavior on x {
            SpringAnimation {
                spring: 5.0          // 刚度 ≈ 300（UIKit 参考值）
                damping: 0.35        // 阻尼 ≈ 30
                epsilon: 0.001
            }
        }

        // ---- 用户内容容器 ----
        Item {
            id: contentContainer
            anchors.fill: parent
            anchors.leftMargin: 20
        }

        // ---- 底部分割线（项目主题色） ----
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
        // 手势识别 (MouseArea 同时处理触摸 + 鼠标)
        // ============================================================

        MouseArea {
            id: gestureArea
            anchors.fill: parent

            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.ArrowCursor
            hoverEnabled: false

            // ---- 按压 ----
            onPressed: function(mouse) {
                if (!root.swipeEnabled || root._deleting) return

                // 增加手势 ID，避免 onReleased 处理过期事件
                root._gestureId++
                var gid = root._gestureId

                // 记录起始位置
                root._startMouseX = mouse.x
                root._startMouseY = mouse.y
                root._startContentX = contentArea.x
                root._horizontalDrag = false
                root._dragConsumed = false

                // ----- 互斥处理 -----
                // 如果有其它项展开且不是自己，立即收回它
                if (root.swipeManager && root.swipeManager.currentItem
                        && root.swipeManager.currentItem !== root) {
                    root.swipeManager.currentItem.collapse()
                }
            }

            // ---- 移动 ----
            onPositionChanged: function(mouse) {
                if (!root.swipeEnabled || root._deleting || root._dragConsumed) return
                if (mouse.buttons !== Qt.LeftButton) return

                var dx = mouse.x - root._startMouseX
                var dy = mouse.y - root._startMouseY

                // 一旦认定是水平拖拽，或正在判断方向
                if (!root._horizontalDrag) {
                    // 需要一定死区避免误触（3px）
                    var absDx = Math.abs(dx)
                    var absDy = Math.abs(dy)

                    if (absDx < 3 && absDy < 3) return

                    if (absDx > absDy) {
                        // ---- 认定为水平滑动 ----
                        root._horizontalDrag = true
                        mouse.accepted = true

                        // 锁定父级垂直滚动
                        if (root.parentFlickable) {
                            root.parentFlickable.interactive = false
                        }

                        // 通知管理器自己正在展开
                        if (root.swipeManager) {
                            root.swipeManager.currentItem = root
                        }
                        root.expandStarted()
                    } else {
                        // ---- 垂直滑动，不消费事件，交由父级 Flickable 处理 ----
                        mouse.accepted = false
                        return
                    }
                }

                // ---- 执行水平拖拽 ----
                if (root._horizontalDrag) {
                    mouse.accepted = true

                    var newX = root._startContentX + dx

                    // 边界限制：允许少量回弹 Overscroll（20px）
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

                    if (!root._expanded) {
                        // 从左向右滑 → 露出按钮
                        if (currentX < -root.threshold) {
                            // 超过阈值 → 完全展开
                            root._expanded = true
                            contentArea.x = -root.buttonWidth
                        } else {
                            // 未超过阈值 → 弹回
                            root._expanded = false
                            contentArea.x = 0
                        }
                    } else {
                        // 从右向左滑（按钮已露出）→ 收回
                        if (currentX > -(root.buttonWidth - root.threshold)) {
                            // 滑回超过阈值 → 完全隐藏
                            root._expanded = false
                            contentArea.x = 0
                            root.expandEnded()
                        } else {
                            // 未超过 → 弹回完全露出
                            root._expanded = true
                            contentArea.x = -root.buttonWidth
                        }
                    }

                    // 恢复父级垂直滚动
                    if (root.parentFlickable) {
                        root.parentFlickable.interactive = true
                    }
                }

                root._horizontalDrag = false
                root._dragConsumed = false
            }

            // ---- 点击（短按而非拖拽） ----
            onClicked: function(mouse) {
                if (!root.swipeEnabled || root._deleting) return

                if (root._expanded) {
                    // 展开状态下点击内容区域 → 收起
                    root.collapse()
                    mouse.accepted = true
                } else {
                    // 未展开的普通点击 → 发射 contentClicked 信号
                    root.contentClicked()
                    mouse.accepted = false
                }
            }

        }
    }

    // ============================================================
    // 管理器的互斥监听
    // ============================================================

    // 保存本地引用，避免销毁后访问 root 时崩溃
    property QtObject _connectedManager: null

    onSwipeManagerChanged: {
        // 断开旧连接
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
        // 组件已销毁时 root 为 null，安全兜底
        if (!_connectedManager) return
        if (_connectedManager.currentItem !== root && root._expanded) {
            root.collapse()
        }
    }

    // ============================================================
    // 公开方法
    // ============================================================

    /// 程序化展开（露出删除按钮）
    function expand() {
        if (!root.swipeEnabled || root._deleting) return

        // 互斥处理
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

    /// 程序化收起（隐藏删除按钮）
    function collapse() {
        root._expanded = false
        contentArea.x = 0
        root.expandEnded()

        // 如果管理器指向自己，清空引用
        if (root.swipeManager && root.swipeManager.currentItem === root) {
            root.swipeManager.currentItem = null
        }
    }
}
