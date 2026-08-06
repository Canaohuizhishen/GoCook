import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    title: qsTr("营养报告")

    property int recipeId: 0

    // 安全属性：等 nutritionReport 加载完之前用空对象兜底，避免 per_serving.* 的 TypeError
    property string loadError: ""
    property var _stackView: null
    readonly property var _report: recipeVM.nutritionReport || {}
    readonly property var _perServing: _report.per_serving || {}
    readonly property var _breakdown: _report.ingredients_breakdown || []

    Component.onCompleted: {
        if (recipeId > 0)
            recipeVM.loadNutritionReport(recipeId)
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("加载营养报告...")
        isLoading: recipeVM.nutritionLoading && Object.keys(_report).length === 0
    }

    // 浮动导航栏
    Rectangle {
        id: navBar
        width: parent.width
        height: 44
        z: 10
        color: Theme.cardBackground

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.dividerColor
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 0

            ToolButton {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                flat: true
                contentItem: Canvas {
                    width: 22
                    height: 22
                    property color arrowColor: Theme.textPrimary
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
                onClicked: _stackView.pop()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("详细营养报告")
                font.pointSize: Theme.fontSizeBody
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
                elide: Text.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
            }

            Item { Layout.preferredWidth: 44 }
        }
    }

    // 主内容
    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.topMargin: navBar.height
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + Theme.spacingLarge * 2
        clip: true

        Column {
            id: contentColumn
            width: parent.width - Theme.spacingMedium * 2
            x: Theme.spacingMedium
            y: Theme.spacingMedium
            spacing: Theme.spacingMedium

            // 菜谱名称
            Text {
                width: parent.width
                text: _report.recipeName || qsTr("营养报告")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH2
                font.weight: Theme.fontWeightBold
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                visible: Object.keys(_report).length > 0
            }

            // 每份营养成分
            Text {
                text: qsTr("每份营养成分")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
                visible: Object.keys(_report).length > 0
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
                visible: Object.keys(_report).length > 0
            }

            // 7 项营养指标
            Grid {
                columns: 2
                width: parent.width
                spacing: 6
                visible: Object.keys(_report).length > 0

                Text { text: qsTr("热量"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeBody }
                Text { text: _perServing.calories + " kcal"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium }

                Text { text: qsTr("蛋白质"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeBody }
                Text { text: _perServing.protein_g + " g"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium }

                Text { text: qsTr("脂肪"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeBody }
                Text { text: _perServing.fat_g + " g"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium }

                Text { text: qsTr("碳水化合物"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeBody }
                Text { text: _perServing.carbs_g + " g"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium }

                Text { text: qsTr("膳食纤维"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeBody }
                Text { text: _perServing.fiber_g + " g"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium }

                Text { text: qsTr("钠"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeBody }
                Text { text: _perServing.sodium_mg + " mg"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium }

                Text { text: qsTr("维生素C"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeBody }
                Text { text: _perServing.vitamin_c_mg + " mg"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium }
            }

            // 食材营养贡献
            Column {
                width: parent.width
                spacing: Theme.spacingSmall
                visible: Object.keys(_report).length > 0
                    && _breakdown
                    && _breakdown.length > 0

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.dividerColor
                }

                Text {
                    text: qsTr("食材营养贡献")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeH3
                    font.weight: Theme.fontWeightMedium
                    color: Theme.textPrimary
                }

                Repeater {
                    model: _breakdown || []
                    delegate: Rectangle {
                        width: parent.width
                        height: ingredientRow.implicitHeight + 8
                        color: "transparent"

                        Column {
                            id: ingredientRow
                            width: parent.width
                            spacing: 2

                            Text {
                                text: modelData.name
                                font.pointSize: Theme.fontSizeBody
                                font.weight: Theme.fontWeightMedium
                                color: Theme.textPrimary
                            }

                            Row {
                                spacing: Theme.spacingSmall

                                Text { text: qsTr("热量") + " " + modelData.calories + " kcal"; color: Theme.textSecondary; font.pointSize: Theme.fontSizeCaption }
                                Text { text: "|"; color: Theme.dividerColor; font.pointSize: Theme.fontSizeCaption }
                                Text { text: qsTr("蛋白") + " " + modelData.protein_g + "g"; color: Theme.textSecondary; font.pointSize: Theme.fontSizeCaption }
                                Text { text: "|"; color: Theme.dividerColor; font.pointSize: Theme.fontSizeCaption }
                                Text { text: qsTr("脂肪") + " " + modelData.fat_g + "g"; color: Theme.textSecondary; font.pointSize: Theme.fontSizeCaption }
                                Text { text: "|"; color: Theme.dividerColor; font.pointSize: Theme.fontSizeCaption }
                                Text { text: qsTr("碳水") + " " + modelData.carbs_g + "g"; color: Theme.textSecondary; font.pointSize: Theme.fontSizeCaption }
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.dividerColor
                            opacity: 0.3
                        }
                    }
                }
            }

            // 健康提示
            Column {
                width: parent.width
                spacing: Theme.spacingXSmall
                visible: Object.keys(_report).length > 0
                    && _report.health_notes !== ""

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.dividerColor
                }

                Text {
                    text: qsTr("健康提示")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeH3
                    font.weight: Theme.fontWeightMedium
                    color: Theme.textPrimary
                }

                Text {
                    id: healthNotesText
                    width: parent.width
                    text: _report.health_notes || ""
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.textHint
                    wrapMode: Text.WordWrap
                }
            }

            // 无数据提示（加载失败与真实无报告区分开：失败优先显示错误）
            Text {
                width: parent.width
                text: loadError !== "" ? loadError : qsTr("该菜谱暂无营养报告")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: loadError !== "" ? Theme.errorColor : Theme.textHint
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: !recipeVM.nutritionLoading && Object.keys(_report).length === 0
            }
        }
    }

    // 加载失败反馈（getRecipeNutrition 已抑制全局提示，此处页内呈现，区分「暂无报告」空状态）
    Connections {
        target: recipeVM
        function onErrorOccurred(error) {
            loadError = error
        }
    }
}
