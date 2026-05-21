import QtQuick
import QtQuick.Controls
import client.styles

Rectangle {
    id: gridCard

    property alias recipeName: nameLabel.text
    property alias imageSource: recipeImage.source
    property alias prepTime: prepTimeLabel.text

    // 推荐专用属性（公共食谱中不显示）
    property real matchScore: 0.0
    property int availableCount: 0
    property int missingCount: 0
    property bool showMatch: false

    signal clicked()
    signal addMissingToCart()

    radius: Theme.radiusMedium
    color: Theme.cardBackground

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 1
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        anchors.bottomMargin: -1
        radius: gridCard.radius
        color: Theme.cardShadowColor
        z: -1
    }

    MouseArea {
        anchors.fill: parent
        onClicked: gridCard.clicked()
    }

    Column {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            width: parent.width
            height: width
            color: Theme.dividerColor
            clip: true
            radius: Theme.radiusSmall

            Image {
                id: recipeImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                source: gridCard.imageSource || ""
                asynchronous: true

                // ── 匹配度徽章（推荐模式） ──
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 4
                    width: matchBadge.implicitWidth + 10
                    height: matchBadge.implicitHeight + 4
                    radius: 8
                    color: Theme.primaryColor
                    opacity: 0.85
                    visible: gridCard.showMatch && gridCard.matchScore > 0
                    Text {
                        id: matchBadge
                        anchors.centerIn: parent
                        text: Math.round(gridCard.matchScore * 100) + "%"
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall - 2
                        font.weight: Font.Bold
                        color: "#fff"
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: Theme.dividerColor
                    visible: recipeImage.status === Image.Error || recipeImage.source === ""
                    Canvas {
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        property color iconColor: Theme.textHint
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1.5
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2 + 2, 10, 0, Math.PI)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(4, height / 2 + 3)
                            ctx.lineTo(width - 4, height / 2 + 3)
                            ctx.stroke()
                        }
                    }
                }
            }
        }

        Column {
            width: parent.width
            leftPadding: Theme.spacingSmall
            rightPadding: Theme.spacingSmall
            topPadding: Theme.spacingSmall - 2
            bottomPadding: Theme.spacingSmall - 2
            spacing: 2

            Text {
                id: nameLabel
                width: parent.width
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                id: prepTimeLabel
                width: parent.width
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeSmall
                color: Theme.textSecondary
                elide: Text.ElideRight
            }

            // ── 食材匹配条（推荐模式） ──
            Row {
                width: parent.width
                spacing: 4
                visible: gridCard.showMatch && (gridCard.availableCount + gridCard.missingCount > 0)

                Rectangle {
                    width: parent.width * (gridCard.availableCount / (gridCard.availableCount + gridCard.missingCount))
                    height: 3
                    radius: 1.5
                    color: Theme.accentColor   // 绿色 = 已有
                }
                Rectangle {
                    width: parent.width * (gridCard.missingCount / (gridCard.availableCount + gridCard.missingCount))
                    height: 3
                    radius: 1.5
                    color: Theme.warningColor
                }
            }

            Row {
                width: parent.width
                spacing: 6
                visible: gridCard.showMatch && (gridCard.availableCount + gridCard.missingCount > 0)

                Text {
                    text: qsTr("已有%1").arg(gridCard.availableCount)
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeSmall - 2
                    color: Theme.accentColor
                }
                Text {
                    text: qsTr("缺%1").arg(gridCard.missingCount)
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeSmall - 2
                    color: Theme.warningColor
                }

                Item { width: 4; height: 1 }
                // ── 快捷加购按钮（仅缺食材时可见） ──
                Rectangle {
                    visible: gridCard.missingCount > 0
                    width: addCartText.implicitWidth + 12
                    height: addCartText.implicitHeight + 4
                    radius: 4
                    color: Theme.primaryColor
                    MouseArea {
                        anchors.fill: parent
                        onClicked: gridCard.addMissingToCart()
                    }
                    Text {
                        id: addCartText
                        anchors.centerIn: parent
                        text: qsTr("+购物车")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall - 3
                        font.weight: Font.Bold
                        color: "#fff"
                    }
                }
            }
        }
    }
}
