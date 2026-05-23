import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    title: qsTr("我的评论")

    header: ToolBar {
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
                onClicked: _stackView.pop()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("我的评论")
                font.pointSize: Theme.fontSizeBody
                font.weight: Theme.fontWeightMedium
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                color: Theme.textPrimary
            }
            Item { Layout.preferredWidth: 44 }
        }
    }

    Component.onCompleted: {
        recipeVM.loadMyRatings()
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在加载评论...")
        isLoading: recipeVM.myRatingsLoading && recipeVM.myRatings.length === 0
    }

    // ---- 评论列表 ----
    ListView {
        id: ratingsListView
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingMedium
        clip: true
        visible: recipeVM.myRatings.length > 0

        model: recipeVM.myRatings

        delegate: Item {
            width: ratingsListView.width
            height: contentLayout.implicitHeight + Theme.spacingMedium * 2
            readonly property int _rating: modelData.rating || 0

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMedium
                color: Theme.cardBackground
                border.width: 1
                border.color: Theme.dividerColor
            }

            ColumnLayout {
                id: contentLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingSmall

                // 第一行：菜谱名称 + 评分
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    Text {
                        Layout.fillWidth: true
                        text: modelData.recipeName || ""
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        font.weight: Theme.fontWeightMedium
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    // 评分星星
                    Row {
                        spacing: 2
                        Layout.alignment: Qt.AlignRight

                        Repeater {
                            model: 5
                            delegate: Text {
                                text: (index < _rating) ? "\u2605" : "\u2606"
                                font.pointSize: Theme.fontSizeBody
                                color: (index < _rating) ? "#FFB800" : Theme.textHint
                            }
                        }
                    }
                }

                // 评论内容
                Text {
                    Layout.fillWidth: true
                    text: modelData.comment || ""
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    visible: modelData.comment && modelData.comment.length > 0
                }

                // 第二行：时间 + 已编辑标记
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    Text {
                        Layout.fillWidth: true
                        text: {
                            var t = modelData.createdAt || ""
                            return t.length >= 10 ? t.substring(0, 10) : t
                        }
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.textHint
                    }

                    // 已编辑标记
                    Text {
                        text: qsTr("已编辑")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.primaryColor
                        visible: modelData.updatedAt && modelData.updatedAt !== modelData.createdAt
                    }

                    // 点击查看详情箭头
                    Text {
                        text: "\u203A"
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textHint
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var page = Qt.createComponent("RecipeDetailPage.qml")
                    if (page.status === Component.Ready) {
                        _stackView.push(page, {recipeId: modelData.recipeId})
                    }
                }
            }
        }

        onAtYEndChanged: {
            if (atYEnd && !recipeVM.myRatingsLoading && recipeVM.myRatingsHasMore) {
                recipeVM.loadMyRatingsNextPage()
            }
        }
    }

    // ---- 空状态 ----
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingMedium
        visible: recipeVM.myRatings.length === 0 && !recipeVM.myRatingsLoading

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "\u2606"
            font.pointSize: 48
            color: Theme.textHint
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("你还没有发表过评论")
            color: Theme.textHint
            font.pointSize: Theme.fontSizeBody
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("在菜谱详情页评分即可留下评论")
            color: Theme.textHint
            font.pointSize: Theme.fontSizeCaption
            opacity: 0.6
        }
    }

    // ---- 错误监听 ----
    Connections {
        target: recipeVM
        function onErrorOccurred(error) {
            console.log("MyRatings error:", error)
        }
    }
}
