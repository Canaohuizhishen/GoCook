import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import client

Rectangle {
    id: card

    // 外部可绑定的属性
    property alias recipeName: nameLabel.text
    property alias recipeDescription: descLabel.text
    property alias imageSource: recipeImage.source
    property alias prepTime: prepTimeLabel.text
    property alias cookTime: cookTimeLabel.text
    property alias tags: tagsRepeater.model

    signal clicked()

    width: parent ? parent.width : 300
    height: 105
    radius: Theme.radiusMedium
    color: Theme.cardBackground

    // 阴影
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 2
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        anchors.bottomMargin: -1
        radius: card.radius
        color: Theme.cardShadowColor
        z: -1
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
            Layout.preferredWidth: 88
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
                    Canvas {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        property color iconColor: Theme.textHint
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1.5
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2 + 1, 7, 0, Math.PI)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(3, height / 2 + 2)
                            ctx.lineTo(width - 3, height / 2 + 2)
                            ctx.stroke()
                        }
                    }
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
                font.pointSize: Theme.fontSizeH3
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
                font.pointSize: Theme.fontSizeCaption
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
                    spacing: 4
                    Canvas {
                        width: 14
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter
                        property color iconColor: Theme.textSecondary
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1
                            ctx.beginPath()
                            ctx.arc(7, 7, 5.5, 0, Math.PI * 2)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(7, 7)
                            ctx.lineTo(7, 3.5)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(7, 7)
                            ctx.lineTo(10, 7)
                            ctx.stroke()
                        }
                    }
                    Text {
                        id: prepTimeLabel
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                    }
                }

                // 烹饪时间
                Row {
                    spacing: 4
                    Canvas {
                        width: 14
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter
                        property color iconColor: Theme.textSecondary
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1
                            ctx.beginPath()
                            ctx.moveTo(7, 1.5)
                            ctx.quadraticCurveTo(13, 5, 7, 12)
                            ctx.quadraticCurveTo(1, 5, 7, 1.5)
                            ctx.stroke()
                        }
                    }
                    Text {
                        id: cookTimeLabel
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall
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
                                font.pointSize: Theme.fontSizeSmall - 1
                                color: Theme.textOnPrimary
                            }
                        }
                    }
                }
            }
        }
    }
}