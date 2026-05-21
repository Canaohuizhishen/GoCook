import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    id: shoppingListPage
    title: qsTr("购物清单")

    signal goBack()
    signal showDetailRequest(int listId)

    Component.onCompleted: {
        shoppingListVM.loadShoppingLists()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingSmall

        // 顶部：返回 + 标题 + 新建按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSmall

            ToolButton {
                text: qsTr("← 返回")
                font.pointSize: Theme.fontSizeBody
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.primaryColor
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: shoppingListPage.goBack()
            }

            Item { Layout.fillWidth: true }

            CustomButton {
                id: addButton
                buttonText: qsTr("+ 新建清单")
                buttonType: CustomButton.ButtonType.Primary
                enabled: !shoppingListVM.creating
                onClicked: createDialog.open()
            }
        }

        // 空状态
        Text {
            Layout.fillWidth: true
            text: qsTr("暂无购物清单，点击上方按钮创建")
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

                    delegate: Rectangle {
                        id: delegateRoot
                        width: listColumn.width
                        height: 56
                        radius: Theme.radiusSmall
                        color: Theme.cardBackground

                        // 右侧箭头按钮——锚定在父容器右侧，始终固定（不参与水平滑动）
                        ToolButton {
                            id: arrowBtn
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.spacingSmall
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 36
                            implicitHeight: 36
                            text: "›"
                            font.pointSize: 20
                            z: 2
                            contentItem: Text {
                                text: arrowBtn.text
                                font: arrowBtn.font
                                color: Theme.textHint
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                shoppingListPage.showDetailRequest(modelData.id)
                            }
                        }

                        // 水平滑动内容区域——独立的 Flickable，与外部垂直 Flickable 不冲突
                        // 布局: 上行为清单名称(粗体)，下行为食材数目 + 创建时间(灰色小字，同行)
                        // Flickable 高度自动跟随 Column 内容，通过 anchors.verticalCenter 在 56px 卡片中居中
                        Flickable {
                            id: itemFlick
                            anchors.left: parent.left
                            anchors.right: arrowBtn.left
                            anchors.leftMargin: Theme.spacingSmall
                            anchors.verticalCenter: parent.verticalCenter
                            height: contentHeight
                            clip: true
                            contentWidth: contentColumn.width
                            contentHeight: contentColumn.height
                            flickableDirection: Flickable.HorizontalFlick

                            Column {
                                id: contentColumn
                                spacing: 2

                                Text {
                                    id: nameText
                                    text: modelData.name || ""
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    font.bold: true
                                    color: Theme.textPrimary
                                }

                                Row {
                                    spacing: Theme.spacingSmall

                                    Text {
                                        id: countText
                                        text: qsTr("%1项食材").arg(modelData.itemCount || 0)
                                        font.family: Theme.fontFamily
                                        font.pointSize: Theme.fontSizeCaption
                                        color: Theme.textSecondary
                                    }

                                    Text {
                                        id: timeText
                                        text: modelData.createdAt || ""
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

    function confirmCreate() {
        var name = nameField.text.trim()
        if (name === "") return
        shoppingListVM.createShoppingList(name)
        nameField.clear()
        createDialog.close()
    }
}
