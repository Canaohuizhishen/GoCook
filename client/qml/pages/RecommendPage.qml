import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import client.styles
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

    onSpinningChanged: {
        if (spinning) ringSpin.start()
        else {
            ringSpin.stop()
            ringIndicator.rotation = 0
        }
    }

    Component.onCompleted: {
        recipeVM.loadPublicRecipes()
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在获取推荐菜谱...")
        isLoading: recipeVM.isLoading && recipeVM.recipes.length === 0 && !refreshing
    }

    ListView {
        id: recipeListView
        anchors.fill: parent
        spacing: Theme.spacingSmall
        clip: true
        boundsBehavior: Flickable.DragOverBounds
        leftMargin: Theme.spacingMedium
        rightMargin: Theme.spacingMedium

        model: recipeVM.recipes

        delegate: RecipeCard {
            width: recipeListView.width - recipeListView.leftMargin - recipeListView.rightMargin
            recipeName: modelData.name
            recipeDescription: modelData.description
            imageSource: modelData.imageUrl || ""
            prepTime: modelData.prepTime + qsTr("分钟")
            cookTime: modelData.cookTime + qsTr("分钟")
            tags: modelData.tags || []
            isFavorite: modelData.isFavorite || false

            onClicked: recommendPage.recipeClicked(modelData.id)
            onFavoriteClicked: console.log("Toggle favorite for:", modelData.id)
        }

        footer: Item {
            width: recipeListView.width
            height: recipeVM.hasMore ? 50 : 0
            visible: recipeVM.hasMore

            CustomButton {
                anchors.centerIn: parent
                buttonText: qsTr("加载更多")
                buttonType: CustomButton.ButtonType.Secondary
                onClicked: recipeVM.loadNextPage()
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
    }

    // 下拉圆圈指示器
    Item {
        id: ringIndicator
        width: circleSize
        height: circleSize
        anchors.horizontalCenter: parent.horizontalCenter

        y: refreshing
           ? -recipeListView.contentY - circleSize - holdPadding
           : Math.max(-circleSize, -recipeListView.contentY - circleSize - holdPadding)

        opacity: refreshing
                 ? 1.0
                 : Math.min(-recipeListView.contentY / pullThreshold, 1.0)

        visible: opacity > 0

        Shape {
            id: ringShape
            anchors.fill: parent
            asynchronous: true

            ShapePath {
                strokeColor: Qt.rgba(0.5, 0.5, 0.5, 0.25)
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
        target: recipeListView; property: "contentY"
        to: holdY; duration: 200
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: releaseList
        target: recipeListView; property: "contentY"
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
                recipeListView.contentY = holdY
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
                recipeListView.contentY = holdY
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
                    recipeListView.contentY = holdY
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
