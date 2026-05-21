import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    id: root
    title: qsTr("购物清单详情")

    property int listId: 0

    signal goBack()

    // 各列宽度（取该列最长内容的 implicitWidth + 内边距）
    property real maxNameWidth: 80
    property real maxReqWidth: 50
    property real maxInvWidth: 50
    property real maxBuyWidth: 50
    property real maxUnitWidth: 40

    // 用于测量文本宽度的隐藏 Text 元素
    Text {
        id: nameMeasurer
        visible: false
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeBody
    }

    Connections {
        target: shoppingListVM
        function onCurrentListChanged() {
            var items = shoppingListVM.currentList.items || []
            var maxName = 60, maxReq = 40, maxInv = 40, maxBuy = 40, maxUnit = 30
            for (var i = 0; i < items.length; i++) {
                nameMeasurer.text = items[i].ingredientName || ""
                maxName = Math.max(maxName, nameMeasurer.implicitWidth)

                nameMeasurer.text = String(items[i].requiredQuantity || 0)
                maxReq = Math.max(maxReq, nameMeasurer.implicitWidth)

                nameMeasurer.text = String(items[i].inventoryQuantity || 0)
                maxInv = Math.max(maxInv, nameMeasurer.implicitWidth)

                nameMeasurer.text = String(items[i].toBuyQuantity || 0)
                maxBuy = Math.max(maxBuy, nameMeasurer.implicitWidth)

                nameMeasurer.text = items[i].unit || ""
                maxUnit = Math.max(maxUnit, nameMeasurer.implicitWidth)
            }
            root.maxNameWidth = maxName + 8
            root.maxReqWidth  = maxReq + 14
            root.maxInvWidth  = maxInv + 14
            root.maxBuyWidth  = maxBuy + 14
            root.maxUnitWidth = maxUnit + 10
        }
    }

    Component.onCompleted: {
        shoppingListVM.loadShoppingListDetail(root.listId)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingSmall

        // 顶部：返回 + 标题
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
                onClicked: root.goBack()
            }

            Text {
                text: shoppingListVM.currentList.name || qsTr("购物清单详情")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.bold: true
                color: Theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
                verticalAlignment: Text.AlignVCenter
            }

            CustomButton {
                buttonText: qsTr("+ 添加食材")
                buttonType: CustomButton.ButtonType.Secondary
                onClicked: addItemDialog.open()
            }
        }

        // 表头 + 食材列表——包裹在水平可滑动的 Flickable 中
        // 食材列取所有行中最长名称的宽度，统一对齐
        // 表格总宽 = 最长食材名 + 各数字列之和，窄屏左右滑动查看
        Flickable {
            id: tableFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: tableColumn.width
            contentHeight: tableColumn.height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: tableColumn
                // 宽度由最宽的子行 implicitWidth 自动决定
                spacing: 1

                // 表头行
                Rectangle {
                    height: 32
                    color: Theme.dividerColor
                    radius: Theme.radiusSmall
                    implicitWidth: headerRow.implicitWidth + Theme.spacingSmall * 2

                    Row {
                        id: headerRow
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacingSmall
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        Text {
                            width: maxNameWidth
                            text: qsTr("食材")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: maxReqWidth; height: implicitHeight
                            text: qsTr("需购")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: maxInvWidth; height: implicitHeight
                            text: qsTr("库存")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: maxBuyWidth; height: implicitHeight
                            text: qsTr("建议买")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: maxUnitWidth; height: implicitHeight
                            text: qsTr("单位")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: 40; height: implicitHeight
                            text: qsTr("状态")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                // 食材数据行
                Repeater {
                    model: shoppingListVM.currentList.items || []

                    delegate: Rectangle {
                        height: 40
                        color: Theme.cardBackground
                        implicitWidth: dataRow.implicitWidth + Theme.spacingSmall * 2

                        Row {
                            id: dataRow
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.spacingSmall
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 0

                            Text {
                                width: maxNameWidth
                                text: modelData.ingredientName || ""
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: maxReqWidth; height: implicitHeight
                                text: modelData.requiredQuantity || 0
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: maxInvWidth; height: implicitHeight
                                text: modelData.inventoryQuantity || 0
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: maxBuyWidth; height: implicitHeight
                                text: modelData.toBuyQuantity || 0
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                font.bold: true
                                color: modelData.toBuyQuantity > 0 ? Theme.primaryColor : Theme.textHint
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: maxUnitWidth; height: implicitHeight
                                text: modelData.unit || ""
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: 40; height: implicitHeight
                                text: modelData.checked ? "✓" : "○"
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: modelData.checked ? "green" : Theme.textHint
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
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
        isLoading: shoppingListVM.isLoading && !shoppingListVM.currentList.id
    }

    // 错误提示 + 批量添加结果反馈
    Connections {
        target: shoppingListVM
        function onErrorOccurred(error) {
            console.log("ShoppingList detail error:", error)
        }
        function onBatchAddComplete(message) {
            // 添加成功，表格已自动刷新
            console.log("Batch add success:", message)
        }
        function onBatchAddFailed(error) {
            console.log("Batch add failed:", error)
        }
    }

    // ===== 添加食材对话框 =====
    Dialog {
        id: addItemDialog
        title: qsTr("添加食材")
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
                text: qsTr("添加食材到购物清单")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.bold: true
                color: Theme.textPrimary
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            TextField {
                id: itemNameField
                Layout.fillWidth: true
                placeholderText: qsTr("食材名称（如: 盐）")
                font.pointSize: Theme.fontSizeBody
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                TextField {
                    id: itemQtyField
                    Layout.fillWidth: true
                    placeholderText: qsTr("数量")
                    font.pointSize: Theme.fontSizeBody
                    onTextChanged: {
                        // 只允许数字和一个小数点，最多 5 位整数 + 2 位小数
                        var cleaned = text.replace(/[^0-9.]/g, '')
                        var dotIdx = cleaned.indexOf('.')
                        if (dotIdx !== -1) {
                            var intPart = cleaned.substring(0, dotIdx).substring(0, 5)
                            var decPart = cleaned.substring(dotIdx + 1).replace(/\./g, '').substring(0, 2)
                            cleaned = intPart + '.' + decPart
                        } else {
                            cleaned = cleaned.substring(0, 5)
                        }
                        if (cleaned !== text) text = cleaned
                    }
                }

                TextField {
                    id: itemUnitField
                    Layout.fillWidth: true
                    placeholderText: qsTr("单位（如: 袋）")
                    font.pointSize: Theme.fontSizeBody
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("取消")
                    buttonType: CustomButton.ButtonType.Secondary
                    onClicked: addItemDialog.close()
                }

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("添加")
                    buttonType: CustomButton.ButtonType.Primary
                    enabled: itemNameField.text.trim() !== ""
                             && itemQtyField.text.trim() !== ""
                    onClicked: confirmAddItem()
                }
            }
        }
    }

    function confirmAddItem() {
        var name = itemNameField.text.trim()
        var qty = parseFloat(itemQtyField.text.trim())
        if (name === "" || isNaN(qty) || !isFinite(qty) || qty <= 0 || qty > 99999.99) return

        var item = {
            "ingredient_name": name,
            "quantity": qty,
            "unit": itemUnitField.text.trim()
        }
        shoppingListVM.batchAddShoppingItems(root.listId, [item])

        itemNameField.clear()
        itemQtyField.clear()
        itemUnitField.clear()
        addItemDialog.close()
    }
}
