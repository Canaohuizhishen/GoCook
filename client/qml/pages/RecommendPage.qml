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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        CustomButton {
            id: refreshButton
            Layout.fillWidth: true
            buttonText: qsTr("刷新")
            buttonType: CustomButton.ButtonType.Secondary
            onClicked: recipeVM.refresh()
        }

        ListView {
            id: recipeListView
            Layout.fillWidth: true
            Layout.fillHeight: true
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
        }
    }

    Label {
        id: errorLabel
        anchors.centerIn: parent
        text: ""
        color: Theme.textHint
        visible: recipeVM.recipes.length === 0 && !loadingIndicator.isLoading && errorLabel.text !== ""
    }

    Connections {
        target: recipeVM
        function onErrorOccurred(error) {
            errorLabel.text = error
        }
    }
}
