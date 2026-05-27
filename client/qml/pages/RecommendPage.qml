import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

import client
import "../components"

Page {
    id: recommendPage
    title: qsTr("推荐")

    signal recipeClicked(int recipeId)

    property real pullThreshold: 60
    property real circleSize: 24
    property real holdPadding: 10
    property real holdY: -(holdPadding + circleSize + holdPadding)
    property bool refreshing: false
    property bool readyToRelease: false
    property bool spinning: false

    readonly property real gridHMargin: Theme.spacingMedium
    readonly property real gridCellWidth: (width - gridHMargin * 2) / 2
    readonly property real gridCellHeight: gridCellWidth + 50

    onSpinningChanged: {
        if (spinning) ringSpin.start()
        else {
            ringSpin.stop()
            ringIndicator.rotation = 0
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

        onMovementEnded: {
            if (refreshing || recipeVM.isLoading) return
            if (contentY < -pullThreshold) {
                refreshing = true
                readyToRelease = false
                spinning = true
                refreshTimer.start()
                minSpinTimer.start()
                recipeVM.refresh()
                snapHold.start()
            } else if (contentY < 0) {
                releaseList.start()
            }
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

        y: refreshing
           ? -recipeGridView.contentY - circleSize - holdPadding
           : Math.max(-circleSize, -recipeGridView.contentY - circleSize - holdPadding)

        opacity: refreshing
                 ? 1.0
                 : Math.min(-recipeGridView.contentY / pullThreshold, 1.0)

        visible: opacity > 0

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
            property: "rotation"
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
