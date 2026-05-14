import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import client.styles

Rectangle {
    id: card

    // 外部可绑定的属性
    property alias recipeName: nameLabel.text
    property alias recipeDescription: descLabel.text
    property alias imageSource: recipeImage.source
    property alias prepTime: prepTimeLabel.text
    property alias cookTime: cookTimeLabel.text
    property alias tags: tagsRepeater.model
    property bool isFavorite: false

    signal clicked()
    signal favoriteClicked()

    width: parent ? parent.width : 300
    height: 120
    radius: Theme.radiusMedium
    color: Theme.cardBackground

    // 阴影
    layer.enabled: true
    layer.effect: DropShadow {
        verticalOffset: 2
        radius: 6
        samples: 13
        color: "#10000000"
    }

    // 点击交互
    MouseArea {
        anchors.fill: parent
        onClicked: card.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        spacing: Theme.spacingMedium

        // 左侧图片区域
        Rectangle {
            Layout.preferredWidth: 100
            Layout.fillHeight: true
            radius: Theme.radiusSmall
            color: Theme.dividerColor
            clip: true

            Image {
                id: recipeImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                source: modelData.imageUrl || ""
                asynchronous: true

                Rectangle {
                    anchors.fill: parent
                    color: Theme.dividerColor
                    visible: recipeImage.status === Image.Error || recipeImage.source === ""
                    Text {
                        anchors.centerIn: parent
                        text: "\U0001F372"
                        font.pixelSize: 32
                    }
                }
            }

            // 收藏按钮（叠加在图片右上角）
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.spacingXSmall
                width: 28
                height: 28
                radius: 14
                color: card.isFavorite ? Theme.errorColor : "#80000000"

                Text {
                    anchors.centerIn: parent
                    text: card.isFavorite ? "❤️" : "🤍"
                    font.pixelSize: 16
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: card.favoriteClicked()
                }
            }
        }

        // 右侧信息区域
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingXSmall

            // 菜谱名称
            Text {
                id: nameLabel
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // 简介
            Text {
                id: descLabel
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textSecondary
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
            }

            // 时间和标签行
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                // 准备时间
                Row {
                    spacing: 2
                    Text { text: "⏱️"; font.pixelSize: 12 }
                    Text {
                        id: prepTimeLabel
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                    }
                }

                // 烹饪时间
                Row {
                    spacing: 2
                    Text { text: "🔥"; font.pixelSize: 12 }
                    Text {
                        id: cookTimeLabel
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                    }
                }

                Item { Layout.fillWidth: true } // 占位弹性空间

                // 标签列表（动态）
                Flow {
                    Layout.preferredWidth: 80
                    spacing: 2

                    Repeater {
                        id: tagsRepeater
                        delegate: Rectangle {
                            width: tagText.implicitWidth + 8
                            height: 18
                            radius: 9
                            color: Theme.primaryLightColor
                            Text {
                                id: tagText
                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: Theme.fontSizeSmall - 1
                                color: "white"
                            }
                        }
                    }
                }
            }
        }
    }
}