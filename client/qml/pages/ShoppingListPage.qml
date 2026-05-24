import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: shoppingListPage
    title: qsTr("购物清单")

    signal goBack()
    signal showDetailRequest(int listId)

    // 用于测量文本宽度的隐藏 Text
    Text {
        id: deleteMeasurer
        visible: false
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeBody
    }

    // 滑动互斥管理器 —— 同一时刻仅一个列表项可展开
    QtObject {
        id: swipeState
        property Item currentItem: null
    }

    Component.onCompleted: {
        shoppingListVM.loadShoppingLists()
    }

    // ========== 顶部导航栏 ==========
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

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
                    onClicked: shoppingListPage.goBack()
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("购物清单")
                    font.pointSize: Theme.fontSizeBody
                    font.weight: Theme.fontWeightMedium
                    elide: Label.ElideRight
                    horizontalAlignment: Qt.AlignHCenter
                    color: Theme.textPrimary
                }
                Item {
                    Layout.preferredWidth: 44
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.spacingMedium
            Layout.rightMargin: Theme.spacingMedium
            Layout.topMargin: Theme.spacingMedium
            Layout.bottomMargin: 56
            spacing: Theme.spacingSmall

            // 空状态
            Text {
                Layout.fillWidth: true
                text: qsTr("暂无购物清单，点击下方按钮创建")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: Theme.textHint
                horizontalAlignment: Text.AlignHCenter
                visible: !shoppingListVM.isLoading && shoppingListVM.shoppingLists.length === 0
            }

            // 购物清单列表——Flickable(垂直) + Repeater，避免嵌套 ListView+Flickable 冲突
            Flickable {
                id: scrollArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentHeight: listColumn.height
                visible: shoppingListVM.shoppingLists.length > 0

                Column {
                    id: listColumn
                    width: parent.width
                    spacing: Theme.spacingXSmall

                    Repeater {
                        model: shoppingListVM.shoppingLists

                        delegate: SwipeToDeleteItem {
                            width: listColumn.width
                            height: 72
                            parentFlickable: scrollArea
                            swipeManager: swipeState

                            onContentClicked: {
                                shoppingListPage.showDetailRequest(modelData.id)
                            }

                            onDeleteRequested: {
                                shoppingListVM.deleteShoppingListOptimistic(modelData.id, modelData)
                            }

                            // 内容区：购物袋图标 + 清单信息
                            RowLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 20

                                // 左侧 Canvas 购物车图标（🛒 等宽还原）
                                Canvas {
                                    id: cartIcon
                                    width: 18
                                    height: 18
                                    property color iconColor: Theme.textSecondary
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.strokeStyle = iconColor
                                        ctx.fillStyle = iconColor
                                        ctx.lineWidth = 1.2
                                        ctx.lineCap = "round"
                                        ctx.lineJoin = "round"

                                        // —— 车筐主体（矩形，底部略收） ——
                                        ctx.beginPath()
                                        ctx.moveTo(2, 7)      // 左上
                                        ctx.lineTo(16, 7)     // 右上
                                        ctx.lineTo(15, 14)    // 右下
                                        ctx.lineTo(3, 14)     // 左下
                                        ctx.closePath()
                                        ctx.stroke()

                                        // —— 车筐内部横线（金属网纹理） ——
                                        ctx.beginPath()
                                        ctx.moveTo(3, 9)
                                        ctx.lineTo(15, 9)
                                        ctx.stroke()

                                        ctx.beginPath()
                                        ctx.moveTo(3, 11.5)
                                        ctx.lineTo(15, 11.5)
                                        ctx.stroke()

                                        // —— 把手（倒U形，从车筐顶部延伸） ——
                                        ctx.beginPath()
                                        ctx.moveTo(5, 7)      // 把手左端
                                        ctx.lineTo(5, 4)      // 把手上弯
                                        ctx.lineTo(13, 4)     // 把手横梁
                                        ctx.lineTo(13, 7)     // 把手右端
                                        ctx.stroke()

                                        // —— 左轮（实心小圆） ——
                                        ctx.beginPath()
                                        ctx.arc(5, 16, 1.5, 0, Math.PI * 2)
                                        ctx.fill()

                                        // —— 右轮（实心小圆） ——
                                        ctx.beginPath()
                                        ctx.arc(13, 16, 1.5, 0, Math.PI * 2)
                                        ctx.fill()
                                    }
                                }

                                // 文字信息
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 2

                                    Text {
                                        id: nameText
                                        Layout.fillWidth: true
                                        text: modelData.name || ""
                                        font.family: Theme.fontFamily
                                        font.pointSize: Theme.fontSizeBody
                                        color: Theme.textPrimary
                                        elide: Text.ElideRight
                                    }

                                    RowLayout {
                                        spacing: Theme.spacingSmall

                                        Text {
                                            text: qsTr("%1项食材").arg(modelData.itemCount || 0)
                                            font.family: Theme.fontFamily
                                            font.pointSize: Theme.fontSizeCaption
                                            color: Theme.textSecondary
                                        }

                                        Text {
                                            text: "·"
                                            font.family: Theme.fontFamily
                                            font.pointSize: Theme.fontSizeCaption
                                            color: Theme.textHint
                                        }

                                        Text {
                                            text: {
                                                var t = modelData.createdAt || ""
                                                if (!t) return ""
                                                var d = new Date(t)
                                                var now = new Date()
                                                var diff = (now - d) / 1000
                                                if (diff < 60) return qsTr("刚刚")
                                                if (diff < 3600) return qsTr("%1分钟前").arg(Math.floor(diff / 60))
                                                if (diff < 86400) return qsTr("%1小时前").arg(Math.floor(diff / 3600))
                                                return qsTr("%1天前").arg(Math.floor(diff / 86400))
                                            }
                                            font.family: Theme.fontFamily
                                            font.pointSize: Theme.fontSizeCaption
                                            color: Theme.textHint
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

    }

    // ========== 底部操作栏 ==========
    Rectangle {
        id: bottomBar
        anchors.bottom: parent.bottom
        width: parent.width
        height: 56
        z: 10
        color: Theme.cardBackground

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Theme.dividerColor
        }

        CustomButton {
            id: addButton
            anchors.centerIn: parent
            width: parent.width - Theme.spacingMedium * 2
            buttonText: qsTr("+ 新建清单")
            buttonType: CustomButton.ButtonType.Primary
            enabled: !shoppingListVM.creating
            onClicked: createDialog.open()
        }
    }

    // 加载指示器
    LoadingIndicator {
        fullscreen: true
        message: qsTr("加载中...")
        isLoading: shoppingListVM.isLoading && shoppingListVM.shoppingLists.length === 0
    }

    // 错误提示
    Connections {
        target: shoppingListVM
        function onErrorOccurred(error) {
            console.log("ShoppingList error:", error)
        }
        function onShoppingListCreated(name) {
            console.log("Shopping list created:", name)
        }
        function onShoppingListCreateFailed(error) {
            console.log("ShoppingList create failed:", error)
        }
        function onShoppingListDeleted(listId) {
            console.log("ShoppingList deleted:", listId)
        }
    }

    // ===== 创建购物清单对话框 =====
    Dialog {
        id: createDialog
        title: qsTr("新建购物清单")
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(parent.width * 0.85, 340)

        background: Rectangle {
            color: Theme.cardBackground
            radius: Theme.radiusMedium
            border.color: Theme.dividerColor
            border.width: 1
        }

        ColumnLayout {
            spacing: Theme.spacingSmall
            width: parent.width

            Text {
                text: qsTr("新建购物清单")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.bold: true
                color: Theme.textPrimary
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("清单名称（如: 周末采购）")
                font.pointSize: Theme.fontSizeBody
                onAccepted: confirmCreate()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("取消")
                    buttonType: CustomButton.ButtonType.Secondary
                    onClicked: createDialog.close()
                }

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("创建")
                    buttonType: CustomButton.ButtonType.Primary
                    enabled: nameField.text.trim() !== "" && !shoppingListVM.creating
                    onClicked: confirmCreate()
                }
            }
        }
    }

    // ===== 删除确认对话框 =====
    Dialog {
        id: deleteDialog
        title: qsTr("确认删除")
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(parent.width * 0.9, Math.max(300, deleteMeasurer.implicitWidth + 80))

        property int listId: 0
        property string listName: ""
        onListNameChanged: {
            deleteMeasurer.text = qsTr("确定要删除「%1」吗？此操作不可撤销。").arg(listName)
        }

        background: Rectangle {
            color: Theme.cardBackground
            radius: Theme.radiusMedium
            border.color: Theme.dividerColor
            border.width: 1
        }

        ColumnLayout {
            spacing: Theme.spacingSmall
            width: parent.width

            Text {
                text: qsTr("确认删除")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.bold: true
                color: Theme.textPrimary
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                text: qsTr("确定要删除「%1」吗？此操作不可撤销。").arg(deleteDialog.listName)
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: Theme.textSecondary
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("取消")
                    buttonType: CustomButton.ButtonType.Secondary
                    onClicked: deleteDialog.close()
                }

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("删除")
                    buttonType: CustomButton.ButtonType.Primary
                    buttonColor: "#d32f2f"
                    onClicked: {
                        shoppingListVM.deleteShoppingList(deleteDialog.listId)
                        deleteDialog.close()
                    }
                }
            }
        }
    }

    function confirmCreate() {
        var name = nameField.text.trim()
        if (name === "") return
        shoppingListVM.createShoppingList(name)
        nameField.clear()
        createDialog.close()
    }
}
