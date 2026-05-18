import QtQuick
import QtQuick.Controls
import client.styles

Rectangle {
    id: gridCard

    property alias recipeName: nameLabel.text
    property alias imageSource: recipeImage.source
    property alias prepTime: prepTimeLabel.text

    signal clicked()

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
        }
    }
}
