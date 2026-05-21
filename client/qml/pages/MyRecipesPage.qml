import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("我的投稿")

    property int currentFilterIndex: 0

    /// 筛选选项
    readonly property var filterOptions: [
        { label: qsTr("全部"),   value: "" },
        { label: qsTr("待审核"), value: "pending" },
        { label: qsTr("已通过"), value: "approved" },
        { label: qsTr("未通过"), value: "rejected" }
    ]

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
                text: qsTr("我的投稿")
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
        recipeVM.loadMyRecipes(1, 20, "")
    }

    /// 状态标签颜色
    function statusColor(status) {
        switch (status) {
            case "pending":  return "#F59E0B"
            case "approved": return "#10B981"
            case "rejected": return "#EF4444"
            default:         return Theme.textHint
        }
    }

    /// 状态中文名
    function statusLabel(status) {
        switch (status) {
            case "pending":  return qsTr("待审核")
            case "approved": return qsTr("已通过")
            case "rejected": return qsTr("未通过")
            default:         return status
        }
    }

    // ---- 顶部状态筛选栏 ----
    RowLayout {
        id: filterBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingSmall

        Repeater {
            model: filterOptions

            delegate: Rectangle {
                id: filterBtn
                height: 32
                implicitWidth: filterLabel.implicitWidth + 20
                radius: Theme.radiusSmall
                color: currentFilterIndex === index ? Theme.primaryColor : Theme.cardBackground
                border.width: currentFilterIndex === index ? 0 : 1
                border.color: Theme.dividerColor

                Text {
                    id: filterLabel
                    anchors.centerIn: parent
                    text: modelData.label
                    font { family: Theme.fontFamily; pointSize: Theme.fontSizeCaption }
                    color: currentFilterIndex === index ? "#FFFFFF" : Theme.textPrimary
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        currentFilterIndex = index
                        recipeVM.loadMyRecipes(1, 20, modelData.value)
                    }
                }
            }
        }
    }

    // ---- 加载指示器 ----
    LoadingIndicator {
        id: loadingIndicator
        anchors.top: filterBar.bottom
        anchors.topMargin: Theme.spacingMedium
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        fullscreen: true
        message: qsTr("正在加载投稿...")
        isLoading: recipeVM.myRecipesLoading && recipeVM.myRecipes.length === 0
    }

    // ---- 投稿列表 ----
    ListView {
        id: myRecipesListView
        anchors.top: filterBar.bottom
        anchors.topMargin: Theme.spacingMedium
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingSmall
        clip: true
        visible: recipeVM.myRecipes.length > 0
        leftMargin: Theme.spacingXSmall
        rightMargin: Theme.spacingXSmall

        model: recipeVM.myRecipes

        delegate: Rectangle {
            width: myRecipesListView.width - myRecipesListView.leftMargin - myRecipesListView.rightMargin
            height: 72
            radius: Theme.radiusMedium
            color: Theme.cardBackground
            border.width: 1
            border.color: Theme.dividerColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingXSmall

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: modelData.name || ""
                            font { family: Theme.fontFamily; pointSize: Theme.fontSizeBody; weight: Theme.fontWeightBold }
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: modelData.submittedAt || ""
                            font { family: Theme.fontFamily; pointSize: Theme.fontSizeCaption }
                            color: Theme.textHint
                        }
                    }

                    // ---- 编辑图标按钮（仅待审核/未通过时显示） ----
                    Rectangle {
                        id: editBtn
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        visible: true
                        width: 22
                        height: 22
                        radius: 11
                        color: mouseEdit.containsMouse ? "#33000000" : "transparent"

                        Canvas {
                            id: editIcon
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.strokeStyle = Theme.textHint
                                ctx.lineWidth = 1.5
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"
                                ctx.beginPath()
                                // 铅笔杆
                                ctx.moveTo(2, 12)
                                ctx.lineTo(10, 4)
                                ctx.lineTo(12, 6)
                                ctx.lineTo(4, 14)
                                ctx.closePath()
                                // 笔尖
                                ctx.moveTo(10, 4)
                                ctx.lineTo(12, 2)
                                ctx.lineTo(14, 4)
                                ctx.lineTo(12, 6)
                                ctx.stroke()
                            }
                        }

                        MouseArea {
                            id: mouseEdit
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: _stackView.push("../pages/SubmitRecipePage.qml",
                                                        { recipeId: modelData.id,
                                                          _stackView: _stackView })
                        }
                    }

                    // ---- 状态标签 ----
                    Rectangle {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        height: 22
                        implicitWidth: statusLabelText.implicitWidth + 12
                        radius: 11
                        color: statusColor(modelData.status || "")

                        Text {
                            id: statusLabelText
                            anchors.centerIn: parent
                            text: statusLabel(modelData.status || "")
                            font { family: Theme.fontFamily; pointSize: Theme.fontSizeSmall }
                            color: "#FFFFFF"
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: modelData.rejectReason !== undefined
                    text: qsTr("拒绝原因: ") + (modelData.rejectReason || "")
                    font { family: Theme.fontFamily; pointSize: Theme.fontSizeCaption }
                    color: "#EF4444"
                    wrapMode: Text.WordWrap
                    Layout.maximumHeight: 36
                    elide: Text.ElideRight
                }
            }
        }

        onAtYEndChanged: {
            if (atYEnd && !recipeVM.myRecipesLoading && recipeVM.myRecipesHasMore) {
                recipeVM.loadMyRecipesNextPage()
            }
        }
    }

    // ---- 空状态 ----
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingMedium
        visible: recipeVM.myRecipes.length === 0 && !recipeVM.myRecipesLoading

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "\uD83D\uDCDD"
            font.pointSize: 48
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("暂无投稿")
            color: Theme.textHint
            font.pointSize: Theme.fontSizeBody
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("在个人中心发布你的第一个菜谱")
            color: Theme.textHint
            font.pointSize: Theme.fontSizeCaption
            opacity: 0.6
        }
    }

    // ---- 错误监听 ----
    Connections {
        target: recipeVM
        function onErrorOccurred(error) {
            console.log("MyRecipes error:", error)
        }
    }
}
