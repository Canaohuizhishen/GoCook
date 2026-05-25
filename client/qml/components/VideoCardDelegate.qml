import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "."

Rectangle {
    id: root
    width: parent ? parent.width : 200
    height: 72
    color: Theme.cardBackground
    radius: Theme.radiusMedium

    property string title: ""
    property string imageUrl: ""
    property string platform: ""
    property int durationSeconds: 0
    signal clicked()

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        spacing: Theme.spacingSmall

        // 缩略图
        Rectangle {
            Layout.preferredWidth: 96
            Layout.preferredHeight: 54
            radius: Theme.radiusSmall
            color: Theme.dividerColor
            clip: true

            Image {
                id: thumbImage
                anchors.fill: parent
                source: root.imageUrl || ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready

                Rectangle {
                    anchors.fill: parent
                    color: Theme.dividerColor
                    visible: thumbImage.status === Image.Error || thumbImage.source === ""

                    Text {
                        anchors.centerIn: parent
                        text: "\uD83C\uDFAC"
                        font.pointSize: 16
                    }
                }
            }
        }

        Column {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            Text {
                width: parent.width
                text: root.title
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
            }

            RowLayout {
                spacing: Theme.spacingXSmall

                Rectangle {
                    radius: Theme.radiusSmall
                    color: root.platform === "youtube" ? "#FF0000" :
                           root.platform === "bilibili" ? "#FB7299" : Theme.dividerColor
                    width: platformText.implicitWidth + 10
                    height: platformText.implicitHeight + 2

                    Text {
                        id: platformText
                        anchors.centerIn: parent
                        text: root.platform
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall
                        color: "#FFFFFF"
                    }
                }

                Text {
                    text: root.durationSeconds > 0 ?
                              Math.floor(root.durationSeconds / 60) + ":" +
                              ("0" + (root.durationSeconds % 60)).slice(-2) : ""
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeSmall
                    color: Theme.textHint
                }
            }
        }
    }
}
