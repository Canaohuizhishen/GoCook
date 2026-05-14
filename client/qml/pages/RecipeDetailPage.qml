import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("菜谱详情")

    property int recipeId: 0

    Component.onCompleted: {
        if (recipeId > 0)
            recipeVM.loadRecipeDetail(recipeId)
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: qsTr("\u2190 返回")
                onClicked: _stackView.pop()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("菜谱详情")
                font.pixelSize: 18
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
            }
            Item { Layout.preferredWidth: 80 }
        }
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在加载菜谱...")
        isLoading: recipeVM.detailLoading
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        contentWidth: width
        contentHeight: detailColumn.implicitHeight + Theme.spacingLarge * 2
        clip: true

        Column {
            id: detailColumn
            width: parent.width - Theme.spacingMedium * 2
            x: Theme.spacingMedium
            spacing: Theme.spacingMedium

            Rectangle {
                width: parent.width
                height: 200
                radius: Theme.radiusMedium
                color: Theme.dividerColor
                clip: true

                Image {
                    id: detailImage
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: recipeVM.recipeDetail.imageUrl || ""
                    asynchronous: true
                }
            }

            Text {
                id: detailName
                width: parent.width
                text: recipeVM.recipeDetail.name || qsTr("加载中...")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeH2
                font.weight: Theme.fontWeightBold
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }

            Flow {
                width: parent.width
                spacing: Theme.spacingSmall
                visible: recipeTags != null && recipeTags.length > 0

                property var recipeTags: recipeVM.recipeDetail.tags || []

                Repeater {
                    model: recipeTags
                    Rectangle {
                        width: tagText.implicitWidth + 12
                        height: 24
                        radius: 12
                        color: Theme.primaryLightColor
                        Text {
                            id: tagText
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: Theme.fontSizeSmall
                            color: "white"
                        }
                    }
                }
            }

            Row {
                width: parent.width
                spacing: Theme.spacingMedium

                Row {
                    spacing: 4
                    Text { text: "\u23F1\uFE0F"; font.pixelSize: 14 }
                    Text {
                        text: qsTr("准备 %1分钟").arg(recipeVM.recipeDetail.prepTime || 0)
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                    }
                }
                Row {
                    spacing: 4
                    Text { text: "🔥"; font.pixelSize: 14 }
                    Text {
                        text: qsTr("烹饪 %1分钟").arg(recipeVM.recipeDetail.cookTime || 0)
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                    }
                }
                Text {
                    text: "|"
                    color: Theme.dividerColor
                }
                Text {
                    text: qsTr("作者: ") + (recipeVM.recipeDetail.authorName || "")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.textSecondary
                }
            }

            Text {
                width: parent.width
                text: recipeVM.recipeDetail.description || ""
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                visible: text !== ""
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
            }

            Text {
                text: qsTr("🥬 食材")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }

            Text {
                id: noIngredientsText
                width: parent.width
                text: qsTr("(暂无食材)")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textHint
                visible: true
            }

            Column {
                id: ingredientsColumn
                width: parent.width
                spacing: 6

                Repeater {
                    id: ingredientsRepeater
                    model: 0
                    delegate: RowLayout {
                        width: ingredientsColumn.width
                        height: 32
                        Text {
                            text: "\u2022  " + modelData.name
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                        }
                        Text {
                            text: modelData.quantity + " " + modelData.unit
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCaption
                            color: Theme.textHint
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
            }

            Text {
                text: qsTr("🍳 步骤")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }

            Text {
                id: noStepsText
                width: parent.width
                text: qsTr("(暂无步骤)")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textHint
                visible: true
            }

            Column {
                id: stepsColumn
                width: parent.width
                spacing: 6

                Repeater {
                    id: stepsRepeater
                    model: 0
                    delegate: ColumnLayout {
                        width: stepsColumn.width
                        spacing: Theme.spacingXSmall

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("步骤 ") + (modelData.order || index + 1)
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Theme.fontWeightMedium
                            color: Theme.primaryColor
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.description
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
                visible: recipeVM.recipeDetail.nutrition && recipeVM.recipeDetail.nutrition.calories > 0
            }

            Column {
                width: parent.width
                spacing: Theme.spacingXSmall
                visible: recipeVM.recipeDetail.nutrition && recipeVM.recipeDetail.nutrition.calories > 0

                Text {
                    text: qsTr("📊 营养信息")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeH3
                    font.weight: Theme.fontWeightMedium
                    color: Theme.textPrimary
                }

                Grid {
                    columns: 2
                    width: parent.width
                    spacing: 4

                    Text { text: qsTr("热量"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                    Text { text: recipeVM.recipeDetail.nutrition.calories + " kcal"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption }
                    Text { text: qsTr("蛋白质"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                    Text { text: recipeVM.recipeDetail.nutrition.protein + " g"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption }
                    Text { text: qsTr("脂肪"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                    Text { text: recipeVM.recipeDetail.nutrition.fat + " g"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption }
                    Text { text: qsTr("碳水"); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeCaption }
                    Text { text: recipeVM.recipeDetail.nutrition.carbs + " g"; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeCaption }
                }
            }
        }
    }

    Connections {
        target: recipeVM
        function onRecipeDetailChanged() {
            var d = recipeVM.recipeDetail
            var ings = (d && d.ingredients) ? d.ingredients : []
            var stps = (d && d.steps) ? d.steps : []
            ingredientsRepeater.model = ings
            stepsRepeater.model = stps
            noIngredientsText.visible = (ings.length === 0)
            noStepsText.visible = (stps.length === 0)
        }
        function onErrorOccurred(error) {
            console.log("RecipeDetail error:", error)
        }
    }
}
