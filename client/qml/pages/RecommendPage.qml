import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("推荐")

    Component.onCompleted: {
        loadingIndicator.isLoading = true
        apiClient.get("/api/recipes/public", function(success, errorMsg, response) {
            loadingIndicator.isLoading = false
            console.log("success:", success)
            console.log("errorMsg:", errorMsg)
            console.log("response type:", typeof response)
            console.log("isArray:", Array.isArray(response))
            console.log("response:", JSON.stringify(response))
            if (success && Array.isArray(response)) {
                for (var i = 0; i < response.length; i++) {
                    var recipe = response[i]
                    recipeModel.append({
                        "id": recipe.id,
                        "name": recipe.name,
                        "description": recipe.description || "",
                        "imageUrl": recipe.image_url || "",
                        "prepTime": recipe.prep_time_minutes || 0,
                        "cookTime": recipe.cook_time_minutes || 0,
                        "tags": recipe.tags || [],
                        "isFavorite": false
                    })
                }
            } else {
                console.error("获取菜谱失败:", errorMsg)
            }
        })
    }

    // 加载指示器（全屏遮罩）
    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在获取推荐菜谱...")
    }

    // 菜谱列表
    ListView {
        id: recipeListView
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        spacing: Theme.spacingSmall
        clip: true

        model: ListModel {
            id: recipeModel
            // 示例数据，后续从 API 获取
        }

        delegate: RecipeCard {
            width: recipeListView.width
            recipeName: model.name
            recipeDescription: model.description
            imageSource: model.imageUrl || ""
            prepTime: model.prepTime + "分钟"
            cookTime: model.cookTime + "分钟"
            tags: model.tags || []
            isFavorite: model.isFavorite || false

            onClicked: {
                // 跳转到详情页
                console.log("Clicked recipe:", model.id)
                // stackView.push("RecipeDetailPage.qml", { recipeId: model.id })
            }

            onFavoriteClicked: {
                // 切换收藏状态
                console.log("Toggle favorite for:", model.id)
                // 调用 ApiClient 收藏接口
            }
        }
    }

    // 占位提示（无数据时）
    Label {
        anchors.centerIn: parent
        text: qsTr("暂无推荐菜谱，请先完善偏好设置")
        color: Theme.textHint
        visible: recipeModel.count === 0 && !loadingIndicator.isLoading
    }

    // Component.onCompleted: {
    //     // 模拟加载数据
    //     loadingIndicator.isLoading = true
    //     // 调用 ApiClient 获取数据，完成后设置 loadingIndicator.isLoading = false
    //     // 并填充 recipeModel
    // }
}