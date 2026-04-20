import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("推荐")

    Component.onCompleted: {
        recipeVM.loadPublicRecipes()
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在获取推荐菜谱...")
        isLoading: recipeVM.isLoading
    }

    ListView {
        id: recipeListView
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        spacing: Theme.spacingSmall
        clip: true

        model: recipeVM.recipes

        delegate: RecipeCard {
            width: recipeListView.width
            recipeName: modelData.name
            recipeDescription: modelData.description
            imageSource: modelData.imageUrl || ""
            prepTime: modelData.prepTime + "分钟"
            cookTime: modelData.cookTime + "分钟"
            tags: modelData.tags || []
            isFavorite: modelData.isFavorite || false

            onClicked: {
                console.log("Clicked recipe:", modelData.id)
            }

            onFavoriteClicked: {
                console.log("Toggle favorite for:", modelData.id)
            }
        }
    }

    Label {
        anchors.centerIn: parent
        text: qsTr("网络不可用")
        color: Theme.textHint
        visible: recipeVM.recipes.length === 0 && !loadingIndicator.isLoading
    }
}