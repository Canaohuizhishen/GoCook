// PullToRefresh — 下拉刷新手势组件（两处共用：首页"推荐" tab、智能推荐结果页）
//
// 用法：
//   PullToRefresh {
//       anchors.fill: parent                    // 覆盖在与 target 同区域、且已 clip 的父项内
//       target: recipeGridView                  // 被拖拽的 Flickable（须 boundsBehavior: DragOverBounds）
//       requestInFlight: recipeVM.isLoading     // 页面绑 VM 加载态：请求在途时不再触发新刷新
//       onRefreshRequested: recipeVM.refresh()  // 手势完成后页面调用对应动作
//   }
//   页面用 <id>.refreshing 抑制居中加载指示器（避免圆环 + spinner 双显）。
//
// 实现要点（勿"顺手优化"）：movementEnded 触发时 Flickable 已把 contentY 回弹为 0，
// 判定必须靠拖拽期打标的 _pulledEnough；刷新态最少旋转 minSpinMs、最长等 maxWaitMs。
// 本组件逐行迁移自 RecommendPage 原内联实现（flickprobe 实测教训保持原样）。
import QtQuick
import QtQuick.Shapes

import client

Item {
    id: root

    // ===== API =====
    /// 被拖拽的滚动目标
    property Flickable target: null
    property real pullThreshold: 60
    /// 页面绑定 VM 加载态：为真时不再触发新的下拉刷新；刷新请求结束后据此收尾
    property bool requestInFlight: false
    /// 下拉手势已触发刷新流程（旋转中 / 等待请求 / 回弹收尾）——页面据此抑制居中 spinner
    readonly property bool refreshing: _refreshing
    /// 用户完成下拉手势（页面接到后调用对应动作）
    signal refreshRequested()

    // ===== 内部状态 =====
    property real circleSize: 24
    property real holdPadding: 10
    property real holdY: -(holdPadding + circleSize + holdPadding)
    property bool _refreshing: false
    property bool _readyToRelease: false
    property bool _spinning: false
    property bool _pulledEnough: false   // 拖拽期已越过下拉阈值（movementEnded 触发时 contentY 已回零，改读此标记）

    /// 目标当前纵向偏移（target 未绑定时的安全回退为 0）
    readonly property real _contentY: target ? target.contentY : 0

    function setSpinning(spin) {
        _spinning = spin
        if (spin) ringSpin.start()
        else {
            ringSpin.stop()
            ringIndicator.spinAngle = 0   // 复位动画角度（rotation 绑定随即切回拖拽进度）
        }
    }

    // 拖拽期越过阈值即打标——onMovementEnded 在超拖回弹结束后才触发（实测 contentY 已归零为 0.0），
    // 不能现读 contentY 判断；回弹只会朝 0 收敛，标记在回弹期间不会被污染。
    Connections {
        target: root.target
        function onContentYChanged() {
            if (!_refreshing && root._contentY < -root.pullThreshold)
                _pulledEnough = true
        }
        function onMovementEnded() {
            if (_refreshing || root.requestInFlight) {
                _pulledEnough = false
                return
            }
            if (_pulledEnough) {
                _refreshing = true
                _readyToRelease = false
                setSpinning(true)
                refreshTimer.start()
                minSpinTimer.start()
                root.refreshRequested()
                snapHold.start()
            }
            _pulledEnough = false
        }
    }

    // 刷新请求结束（页面把 requestInFlight 置回 false）：等最短旋转时长后收尾回弹
    onRequestInFlightChanged: {
        if (_refreshing && !requestInFlight) {
            refreshTimer.stop()
            _readyToRelease = true
            if (!minSpinTimer.running) {
                root.target.contentY = holdY
                setSpinning(false)
                releaseList.start()
            }
        }
    }

    // 下拉圆圈指示器
    Item {
        id: ringIndicator
        width: root.circleSize
        height: root.circleSize
        anchors.horizontalCenter: parent.horizontalCenter

        /// 刷新态持续旋转角度（由 ringSpin 驱动；非刷新态 rotation 直接绑定下拉进度）
        property real spinAngle: 0

        y: _refreshing
           ? -root._contentY - root.circleSize - root.holdPadding
           : Math.max(-root.circleSize, -root._contentY - root.circleSize - root.holdPadding)

        opacity: _refreshing
                 ? 1.0
                 : Math.min(-root._contentY / root.pullThreshold, 1.0)

        visible: opacity > 0

        // 拖拽期按下拉距离转动（2°/px）；进入刷新态无缝切换为 spinAngle 持续旋转
        rotation: _refreshing ? spinAngle : Math.max(0, -root._contentY) * 2

        Shape {
            id: ringShape
            anchors.fill: parent
            asynchronous: true

            ShapePath {
                strokeColor: Theme.dividerColor
                strokeWidth: 2.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap

                PathAngleArc {
                    centerX: root.circleSize / 2; centerY: root.circleSize / 2
                    radiusX: (root.circleSize - 3) / 2; radiusY: (root.circleSize - 3) / 2
                    startAngle: 0; sweepAngle: 360
                }
            }

            ShapePath {
                strokeColor: Theme.primaryColor
                strokeWidth: 2.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap

                PathAngleArc {
                    centerX: root.circleSize / 2; centerY: root.circleSize / 2
                    radiusX: (root.circleSize - 3) / 2; radiusY: (root.circleSize - 3) / 2
                    startAngle: -90; sweepAngle: 270
                }
            }
        }

        NumberAnimation {
            id: ringSpin
            target: ringIndicator
            property: "spinAngle"
            from: 0; to: 360
            duration: 800
            loops: Animation.Infinite
        }
    }

    NumberAnimation {
        id: snapHold
        target: root.target; property: "contentY"
        to: root.holdY; duration: 200
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: releaseList
        target: root.target; property: "contentY"
        to: 0; duration: 250
        easing.type: Easing.OutCubic
        onStopped: { _refreshing = false }
    }

    Timer {
        id: refreshTimer
        interval: 5000
        onTriggered: {
            if (_refreshing) {
                minSpinTimer.stop()
                root.target.contentY = root.holdY
                setSpinning(false)
                releaseList.start()
            }
        }
    }

    Timer {
        id: minSpinTimer
        interval: 1000
        onTriggered: {
            if (_refreshing && _readyToRelease) {
                root.target.contentY = root.holdY
                setSpinning(false)
                releaseList.start()
            }
        }
    }
}
