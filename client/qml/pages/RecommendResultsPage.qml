import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: recResultsPage
    title: qsTr("智能推荐")

    signal recipeClicked(int recipeId, string healthNotice)

    property string recError: ""
    property bool _dataLoaded: false
    property var _pendingCard: null

    Component.onCompleted: {
        recipeVM.loadRecommendedRecipes(1, 20)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── 顶部栏（方块 ToolButton + Canvas 箭头 + 居中标题） ──
        ToolBar {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                ToolButton {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    flat: true
                    contentItem: Canvas {
                        width: 22
                        height: 22
                        property color arrowColor: Theme.textPrimary
                        onArrowColorChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = arrowColor
                            ctx.lineWidth = 2
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.moveTo(14, 5)
                            ctx.lineTo(6, 11)
                            ctx.lineTo(14, 17)
                            ctx.stroke()
                        }
                    }
                    onClicked: {
                        recipeVM.loadPublicRecipes(1, 20)
                        if (_stackView) _stackView.pop()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("智能推荐")
                    font.pointSize: Theme.fontSizeBody
                    font.weight: Theme.fontWeightMedium
                    elide: Label.ElideRight
                    horizontalAlignment: Qt.AlignHCenter
                    color: Theme.textPrimary
                }
                Item { Layout.preferredWidth: 44 }
            }
        }

        // ── 健康过滤横幅 ──
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingMedium
            Layout.rightMargin: Theme.spacingMedium
            Layout.bottomMargin: Theme.spacingSmall
            height: healthText.implicitHeight + 10
            radius: 6
            color: Theme.warningColor
            visible: recipeVM.healthFilterApplied
            opacity: 0.85

            Text {
                id: healthText
                anchors.centerIn: parent
                text: qsTr("⚕ 已根据您的健康档案过滤禁忌食材")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeSmall - 1
                color: "#333"
            }
        }

        // ── 内容区 ──
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 加载中
            LoadingIndicator {
                anchors.centerIn: parent
                fullscreen: false
                message: qsTr("正在智能推荐...")
                isLoading: recipeVM.isLoading
                visible: recipeVM.isLoading
            }

            // 推荐列表
            ListView {
                id: recListView
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMedium
                anchors.rightMargin: Theme.spacingMedium
                spacing: Theme.spacingSmall
                clip: true
                visible: recResultsList.length > 0

                property var recResultsList: []

                model: recResultsList

                delegate: RecipeCard {
                    width: recListView.width - recListView.leftMargin - recListView.rightMargin
                    recipeName: modelData.name
                    recipeDescription: modelData.description
                    imageSource: modelData.imageUrl ? authViewModel.apiBaseUrl + modelData.imageUrl : ""
                    prepTime: (modelData.prepTime || "0") + qsTr("分钟")
                    cookTime: (modelData.cookTime || "0") + qsTr("分钟")
                    tags: modelData.tags || []

                    showMatch: true
                    matchScore: modelData.matchScore || 0
                    healthNotice: modelData.healthNotice || ""
                    availableCount: modelData.matchStatus ? modelData.matchStatus["available_ingredients"].length : 0
                    missingCount: modelData.matchStatus ? modelData.matchStatus["missing_ingredients"].length : 0

                    onClicked: {
                        recResultsPage.recipeClicked(modelData.id, modelData.healthNotice || "")
                    }
                    onAddMissingToCart: {
                        var name = modelData.name || ""
                        var missingIngredients = modelData.matchStatus
                            ? (modelData.matchStatus["missing_ingredients"] || [])
                            : []
                        if (name === "" || missingIngredients.length === 0) return
                        shoppingListVM.createListFromRecipe(name, missingIngredients)
                        recResultsPage._pendingCard = this
                    }
                }
            }

            // 空 / 错误状态
            Column {
                anchors.centerIn: parent
                spacing: Theme.spacingSmall
                visible: !recipeVM.isLoading && recListView.recResultsList.length === 0

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: recError.text || qsTr("暂无推荐结果，请确认库存不为空")
                    color: recError.text ? Theme.errorColor : Theme.textHint
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                }
            }
        }
    }

    // ── 状态监听 ──
    Connections {
        target: recipeVM
        function onRecipesChanged() {
            var list = recipeVM.recipes
            if (list.length > 0 && typeof list[0].matchScore !== 'undefined') {
                recListView.recResultsList = list
                recResultsPage.recError = ""
                recResultsPage._dataLoaded = true
            } else if (!recipeVM.isLoading && !_dataLoaded) {
                recResultsPage.recError = qsTr("无法获取推荐，请确认库存不为空且已登录")
            }
        }
        function onErrorOccurred(error) {
            recResultsPage.recError = error
        }
        function onIsLoadingChanged() {
            if (!recipeVM.isLoading && !_dataLoaded && recResultsPage.recError === "") {
                recResultsPage.recError = qsTr("暂无推荐结果")
            }
        }
    }

    Connections {
        target: shoppingListVM
        function onBatchAddComplete(message) {
            if (recResultsPage._pendingCard) {
                recResultsPage._pendingCard.showCartFeedback(true)
                recResultsPage._pendingCard = null
            }
        }
        function onBatchAddFailed(error) {
            if (recResultsPage._pendingCard) {
                recResultsPage._pendingCard.showCartFeedback(false)
                recResultsPage._pendingCard = null
            }
        }
    }
}
