import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

import client
import "../components"

Page {
    id: recommendPage
    title: qsTr("推荐")

    // 页面内容不得画出页面边界：下拉圆环回缩越过顶边时在页面边界被裁切（从搜索栏下层滑出，
    // HomePage 的搜索栏在 SwipeView 之外且先于其绘制，不裁切会盖到搜索栏之上）
    clip: true

    signal recipeClicked(int recipeId)

    property real pullThreshold: 60
    property real circleSize: 24
    property real holdPadding: 10
    property real holdY: -(holdPadding + circleSize + holdPadding)
    property bool refreshing: false
    property bool readyToRelease: false
    property bool spinning: false
    property bool pulledEnough: false   // 拖拽期已越过下拉阈值（movementEnded 触发时 contentY 已回零，改读此标记）

    readonly property real gridHMargin: Theme.spacingMedium
    readonly property real gridCellWidth: (width - gridHMargin * 2) / 2
    readonly property real gridCellHeight: gridCellWidth + 50

    onSpinningChanged: {
        if (spinning) ringSpin.start()
        else {
            ringSpin.stop()
            ringIndicator.spinAngle = 0   // 复位动画角度（rotation 绑定随即切回拖拽进度）
        }
    }

    Component.onCompleted: {
        recipeVM.loadPublicRecipes(1)
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在获取推荐菜谱...")
        isLoading: recipeVM.isLoading && recipeVM.recipes.length === 0 && !refreshing
    }

    GridView {
        id: recipeGridView
        anchors.fill: parent
        cellWidth: gridCellWidth
        cellHeight: gridCellHeight
        clip: true
        boundsBehavior: Flickable.DragOverBounds
        bottomMargin: Theme.spacingLarge
        leftMargin: gridHMargin
        rightMargin: gridHMargin

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        model: recipeVM.recipes

        delegate: Item {
            width: recipeGridView.cellWidth
            height: recipeGridView.cellHeight

            RecipeGridCard {
                anchors.fill: parent
                anchors.margins: Theme.spacingXSmall
                recipeName: modelData.name
                imageSource: modelData.imageUrl ? authViewModel.apiBaseUrl + modelData.imageUrl : ""
                prepTime: qsTr("约%1分钟").arg(modelData.prepTime + modelData.cookTime)

                onClicked: recommendPage.recipeClicked(modelData.id)
            }
        }

        // 拖拽期越过阈值即打标——onMovementEnded 在超拖回弹结束后才触发（实测 contentY 已归零为 0.0），
        // 不能现读 contentY 判断；回弹只会朝 0 收敛，标记在回弹期间不会被污染。
        onContentYChanged: {
            if (!refreshing && contentY < -pullThreshold)
                pulledEnough = true
        }

        onMovementEnded: {
            if (refreshing || recipeVM.isLoading) {
                pulledEnough = false
                return
            }
            if (pulledEnough) {
                refreshing = true
                readyToRelease = false
                spinning = true
                refreshTimer.start()
                minSpinTimer.start()
                recipeVM.refresh()
                snapHold.start()
            }
            pulledEnough = false
        }

        onAtYEndChanged: {
            if (atYEnd && !recipeVM.isLoading && recipeVM.hasMore) {
                recipeVM.loadNextPage()
            }
        }
    }

    // 下拉圆圈指示器
    Item {
        id: ringIndicator
        width: circleSize
        height: circleSize
        anchors.horizontalCenter: parent.horizontalCenter

        /// 刷新态持续旋转角度（由 ringSpin 驱动；非刷新态 rotation 直接绑定下拉进度）
        property real spinAngle: 0

        y: refreshing
           ? -recipeGridView.contentY - circleSize - holdPadding
           : Math.max(-circleSize, -recipeGridView.contentY - circleSize - holdPadding)

        opacity: refreshing
                 ? 1.0
                 : Math.min(-recipeGridView.contentY / pullThreshold, 1.0)

        visible: opacity > 0

        // 拖拽期按下拉距离转动（2°/px）；进入刷新态无缝切换为 spinAngle 持续旋转
        rotation: refreshing ? spinAngle : Math.max(0, -recipeGridView.contentY) * 2

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
                    centerX: circleSize / 2; centerY: circleSize / 2
                    radiusX: (circleSize - 3) / 2; radiusY: (circleSize - 3) / 2
                    startAngle: 0; sweepAngle: 360
                }
            }

            ShapePath {
                strokeColor: Theme.primaryColor
                strokeWidth: 2.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap

                PathAngleArc {
                    centerX: circleSize / 2; centerY: circleSize / 2
                    radiusX: (circleSize - 3) / 2; radiusY: (circleSize - 3) / 2
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
        target: recipeGridView; property: "contentY"
        to: holdY; duration: 200
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: releaseList
        target: recipeGridView; property: "contentY"
        to: 0; duration: 250
        easing.type: Easing.OutCubic
        onStopped: { refreshing = false }
    }

    Timer {
        id: refreshTimer
        interval: 5000
        onTriggered: {
            if (refreshing) {
                minSpinTimer.stop()
                recipeGridView.contentY = holdY
                spinning = false
                releaseList.start()
            }
        }
    }

    Timer {
        id: minSpinTimer
        interval: 1000
        onTriggered: {
                if (refreshing && readyToRelease) {
                recipeGridView.contentY = holdY
                spinning = false
                releaseList.start()
            }
        }
    }

    Connections {
        target: recipeVM
        function onIsLoadingChanged() {
            if (refreshing && !recipeVM.isLoading) {
                refreshTimer.stop()
                readyToRelease = true
                if (!minSpinTimer.running) {
                    recipeGridView.contentY = holdY
                    spinning = false
                    releaseList.start()
                }
            }
        }
    }

    Label {
        id: errorLabel
        anchors.centerIn: parent
        text: ""
        color: Theme.textHint
        visible: recipeVM.recipes.length === 0
                 && !recipeVM.isLoading
                 && errorLabel.text !== ""
    }

    Connections {
        target: recipeVM
        function onErrorOccurred(error) { errorLabel.text = error }
    }
}
