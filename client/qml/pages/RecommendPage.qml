import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import client
import "../components"

Page {
    id: recommendPage
    title: qsTr("推荐")

    // 页面内容不得画出页面边界：下拉圆环回缩越过顶边时在页面边界被裁切（从搜索栏下层滑出，
    // HomePage 的搜索栏在 SwipeView 之外且先于其绘制，不裁切会盖到搜索栏之上）
    clip: true

    signal recipeClicked(int recipeId)

    readonly property real gridHMargin: Theme.spacingMedium
    readonly property real gridCellWidth: (width - gridHMargin * 2) / 2
    readonly property real gridCellHeight: gridCellWidth + 50

    Component.onCompleted: {
        recipeVM.loadPublicRecipes(1)
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在获取推荐菜谱...")
        isLoading: recipeVM.isLoading && recipeVM.recipes.length === 0 && !pullRefresh.refreshing
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

        onAtYEndChanged: {
            if (atYEnd && !recipeVM.isLoading && recipeVM.hasMore) {
                recipeVM.loadNextPage()
            }
        }
    }

    // 下拉刷新 → 重新加载公开列表（「换一批」语义在推荐结果页；手势逻辑见公共组件）
    PullToRefresh {
        id: pullRefresh
        anchors.fill: parent
        target: recipeGridView
        requestInFlight: recipeVM.isLoading
        onRefreshRequested: recipeVM.refresh()
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
