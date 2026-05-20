import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    title: qsTr("我的收藏")

    Component.onCompleted: {
        recipeVM.loadFavorites()
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在加载收藏...")
        isLoading: recipeVM.favoritesLoading && recipeVM.favorites.length === 0
    }

    ListView {
        id: favoritesListView
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingSmall
        clip: true
        visible: recipeVM.favorites.length > 0
        leftMargin: Theme.spacingXSmall
        rightMargin: Theme.spacingXSmall

        model: recipeVM.favorites

        delegate: RecipeCard {
            width: favoritesListView.width - favoritesListView.leftMargin - favoritesListView.rightMargin
            recipeName: modelData.name
            recipeDescription: modelData.description
            imageSource: modelData.imageUrl || ""
            prepTime: ""
            cookTime: ""
            tags: []

            onClicked: {
                var page = Qt.createComponent("RecipeDetailPage.qml")
                if (page.status === Component.Ready) {
                    _stackView.push(page, {recipeId: modelData.id})
                }
            }
        }

        onAtYEndChanged: {
            if (atYEnd && !recipeVM.favoritesLoading && recipeVM.favoritesHasMore) {
                recipeVM.loadMoreFavorites()
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingMedium
        visible: recipeVM.favorites.length === 0 && !recipeVM.favoritesLoading

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "\u2606"
            font.pointSize: 48
            color: Theme.textHint
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("收藏喜欢的菜谱")
            color: Theme.textHint
            font.pointSize: Theme.fontSizeBody
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("在菜谱详情页点击收藏")
            color: Theme.textHint
            font.pointSize: Theme.fontSizeCaption
            opacity: 0.6
        }
    }

    Connections {
        target: recipeVM
        function onErrorOccurred(error) {
            console.log("Favorites error:", error)
        }
    }
}
