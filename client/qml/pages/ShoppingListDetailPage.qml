import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
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
            // 名称列取实际最长文本宽度，不截断
            var fixedW = maxReq + 14 + maxInv + 14 + maxBuy + 14 + maxUnit + 10 + 40
            var nameW  = Math.max(maxName + 8, 60)
            // 有多余空间时撑满名称列，无多余空间时保持自然宽度（Flickable 可横向滚动）
            var availW = Math.max(root.width - 48, 260)
            if (nameW + fixedW < availW) {
                nameW = availW - fixedW  // 空间有余时撑满
            }
            root.maxNameWidth = nameW
            root.maxReqWidth  = maxReq + 14
            root.maxInvWidth  = maxInv + 14
            root.maxBuyWidth  = maxBuy + 14
            root.maxUnitWidth = maxUnit + 10
        }
    }

    Component.onCompleted: {
        shoppingListVM.loadShoppingListDetail(root.listId)
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
                    onClicked: root.goBack()
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("购物清单详情")
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

            // 清单名称（独占一行，完整显示不截断）
            Text {
                text: shoppingListVM.currentList.name || qsTr("购物清单详情")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                color: Theme.textPrimary
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
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

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
                ScrollBar.horizontal: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Column {
                    id: tableColumn
                    // 宽度由最宽的子行 implicitWidth 自动决定
                    spacing: 1

                    ShoppingListTableHeader {
                        nameWidth: maxNameWidth
                        reqWidth: maxReqWidth
                        invWidth: maxInvWidth
                        buyWidth: maxBuyWidth
                        unitWidth: maxUnitWidth
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
                                    width: maxNameWidth; height: 40
                                    text: modelData.ingredientName || ""
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    color: Theme.textPrimary
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text { width: maxReqWidth; height: 40
                                    text: modelData.requiredQuantity || 0
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    color: Theme.textPrimary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text { width: maxInvWidth; height: 40
                                    text: modelData.inventoryQuantity || 0
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    color: Theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text { width: maxBuyWidth; height: 40
                                    text: modelData.toBuyQuantity || 0
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    font.bold: true
                                    color: modelData.toBuyQuantity > 0 ? Theme.primaryColor : Theme.textHint
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text { width: maxUnitWidth; height: 40
                                    text: modelData.unit || ""
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    color: Theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Item {
                                    width: 40; height: 40
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.checked ? "☑" : "☐"
                                        font.family: Theme.fontFamily
                                        font.pointSize: Theme.fontSizeH3
                                        color: modelData.checked ? Theme.primaryColor : Theme.textHint
                                    }
                                    MouseArea {
                                        id: statusMouseArea
                                        anchors.fill: parent
                                        enabled: !shoppingListVM.isLoading
                                        onClicked: shoppingListVM.updateShoppingListItem(root.listId, modelData.id, !modelData.checked)
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

        RowLayout {
            anchors.centerIn: parent
            width: parent.width - Theme.spacingMedium * 2
            spacing: Theme.spacingSmall

            CustomButton {
                id: exportBtn
                Layout.fillWidth: true
                buttonText: qsTr("导出")
                buttonType: CustomButton.ButtonType.Secondary
                onClicked: exportMenu.open()
            }
            CustomButton {
                id: addIngredientBtn
                Layout.fillWidth: true
                buttonText: qsTr("+ 添加食材")
                buttonType: CustomButton.ButtonType.Secondary
                onClicked: addItemDialog.open()
            }
        }

        Popup {
            id: exportMenu
            y: exportBtn.y - height - 4
            x: exportBtn.x + exportBtn.width / 2 - width / 2
            width: 140
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            padding: 0

            background: Rectangle {
                color: Theme.cardBackground
                radius: Theme.radiusSmall
                border.color: Theme.dividerColor
                border.width: 1
            }

            ColumnLayout {
                spacing: 0
                width: parent.width

                Repeater {
                    model: [
                        { text: qsTr("导出文本"), icon: "📄" },
                        { text: qsTr("导出图片"), icon: "🖼" }
                    ]
                    delegate: Rectangle {
                        id: menuItemRoot
                        Layout.fillWidth: true
                        height: 40
                        color: menuItemMa.pressed ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.15)
                             : menuItemMa.containsMouse ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.08)
                             : "transparent"
                        scale: menuItemMa.pressed ? 0.96 : 1.0
                        Behavior on scale { NumberAnimation { duration: 60; easing.type: Easing.InOutQuad } }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.icon + "  " + modelData.text
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody
                            color: Theme.textPrimary
                            verticalAlignment: Text.AlignVCenter
                        }

                        MouseArea {
                            id: menuItemMa
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                exportMenu.close()
                                if (index === 0) {
                                    shoppingListVM.exportShoppingList(root.listId)
                                } else {
                                    exportAsImage()
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
        isLoading: shoppingListVM.isLoading && !shoppingListVM.currentList.id
    }

    // 错误提示 + 批量添加结果反馈
    Connections {
        target: shoppingListVM
        function onErrorOccurred(error) {
            console.log("ShoppingList detail error:", error)
        }
        function onBatchAddComplete(message) {
            feedbackToast.show(qsTr("✓ 已添加"), "#4caf50")
        }
        function onBatchAddFailed(error) {
            console.log("Batch add failed:", error)
        }
        function onExportReady(content) {
            var ta = textArea
            ta.text = content
            ta.selectAll()
            ta.copy()
            var listName = shoppingListVM.currentList.name || ""
            feedbackToast.show(qsTr("✓ 清单「%1」已复制到剪贴板").arg(listName), Theme.primaryColor)
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

    function exportAsImage() {
        tableColumn.grabToImage(function(result) {
            var listName = (shoppingListVM.currentList.name || "shopping-list").replace(/[\\/:*?\"<>|]/g, "_")
            var timestamp = new Date().toISOString().slice(0, 19).replace(/[:-]/g, "")
            var fileName = "GoCook-" + listName + "-" + timestamp + ".png"
            var filePath = "/tmp/" + fileName
            result.saveToFile(filePath)
            exportDoneDialog.filePath = filePath
            exportDoneDialog.open()
        })
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

    // ===== 导出图片完成对话框 =====
    Dialog {
        id: exportDoneDialog
        title: qsTr("导出完成")
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(parent.width * 0.75, 380)
        height: exportDoneLayout.implicitHeight + 80

        property string filePath: ""

        background: Rectangle {
            color: Theme.cardBackground
            radius: Theme.radiusMedium
            border.color: Theme.dividerColor
            border.width: 1
        }

        ColumnLayout {
            id: exportDoneLayout
            spacing: Theme.spacingMedium
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.top: parent.top
            anchors.topMargin: 20

            Text {
                text: qsTr("图片已保存")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.bold: true
                color: Theme.primaryColor
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                text: exportDoneDialog.filePath
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: Theme.textSecondary
                Layout.fillWidth: true
                wrapMode: Text.WrapAnywhere
                horizontalAlignment: Text.AlignHCenter
            }

            CustomButton {
                Layout.fillWidth: true
                buttonText: qsTr("确定")
                buttonType: CustomButton.ButtonType.Primary
                onClicked: exportDoneDialog.close()
            }
        }
    }

    // 隐藏的 TextArea 用于复制到剪贴板
    TextArea {
        id: textArea
        visible: false
    }

    // 反馈提示条
    Rectangle {
        id: feedbackToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 80
        width: parent.width * 0.75
        height: toastLabel.lineCount > 1 ? 68 : 48
        radius: 18
        visible: false
        opacity: 0
        z: 999

        property string toastMsg: ""
        property color toastClr: Theme.primaryColor

        function show(msg, clr) {
            toastMsg = msg
            toastClr = clr
            visible = true
            opacity = 1.0
            fadeTimer.restart()
        }

        color: toastClr

        Text {
            id: toastLabel
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: feedbackToast.toastMsg
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            color: "white"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Behavior on opacity { NumberAnimation { duration: 400 } }

        Timer {
            id: fadeTimer
            interval: 2000
            onTriggered: {
                feedbackToast.opacity = 0
            }
        }

        onOpacityChanged: {
            if (opacity === 0 && visible) {
                hideTimer.start()
            }
        }
        Timer {
            id: hideTimer
            interval: 400
            onTriggered: {
                feedbackToast.visible = false
            }
        }
    }

}
