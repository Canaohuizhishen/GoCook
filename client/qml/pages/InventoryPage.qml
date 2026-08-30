import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    title: qsTr("库存")

    // 当前正在编辑的库存项ID
    property int currentEditItemId: -1
    property bool addPending: false
    property bool editPending: false

    signal showShoppingListRequest()

    // 页面每次可见时刷新（首次创建与登录/登出后切回均生效），并清空上一次的状态提示，
    // 避免登录成功后仍残留「请先登录」/旧状态
    Component.onCompleted: {
        if (visible) inventoryVM.loadInventory()
    }
    onVisibleChanged: {
        if (visible) {
            statusText.text = ""
            inventoryVM.loadInventory()
        }
    }

    // 按钮组：锚定页面顶部，与内容区解耦——空/非空库存时三个按钮位置严格一致、紧贴顶部
    ColumnLayout {
        id: buttonGroup
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Theme.spacingSmall
        spacing: Theme.spacingSmall

        CustomButton {
            id: addButton
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingMedium
            Layout.rightMargin: Theme.spacingMedium
            buttonText: qsTr("+ 添加食材")
            buttonType: CustomButton.ButtonType.Primary
            onClicked: addDialog.open()
        }

        CustomButton {
            id: recommendButton
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingMedium
            Layout.rightMargin: Theme.spacingMedium
            buttonText: qsTr("✦ 一键智能推荐")
            buttonType: CustomButton.ButtonType.Secondary
            enabled: inventoryVM.items.length > 0
            onClicked: homePage.showRecommendFromInventory()
        }

        CustomButton {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingMedium
            Layout.rightMargin: Theme.spacingMedium
            buttonText: qsTr("☑ 购物清单")
            buttonType: CustomButton.ButtonType.Secondary
            onClicked: showShoppingListRequest()
        }
    }

    // 内容区：按钮组下方到页面底部（空态提示 / 状态提示 / 列表）
    ColumnLayout {
        anchors.top: buttonGroup.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: Theme.spacingSmall

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingMedium
            Layout.rightMargin: Theme.spacingMedium
            text: authViewModel.loggedIn ? qsTr("暂无库存，点击上方按钮添加食材") : qsTr("登录后查看你的库存")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            color: Theme.textHint
            horizontalAlignment: Text.AlignHCenter
            visible: !inventoryVM.isLoading && inventoryVM.items.length === 0
        }

        // ========== 页面级状态提示（兜底，对话框未打开时显示） ==========
        Text {
            id: statusText; Layout.fillWidth: true; height: 20
            Layout.leftMargin: Theme.spacingMedium
            Layout.rightMargin: Theme.spacingMedium
            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
            color: Theme.accentColor; horizontalAlignment: Text.AlignHCenter
            visible: text.length > 0
        }

        ListView {
            id: inventoryListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            leftMargin: Theme.spacingMedium
            rightMargin: Theme.spacingMedium
            spacing: Theme.spacingXSmall
            clip: true
            visible: inventoryVM.items.length > 0

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            model: inventoryVM.items

            delegate: Rectangle {
                width: inventoryListView.width - inventoryListView.leftMargin - inventoryListView.rightMargin
                height: 48
                radius: Theme.radiusSmall
                color: Theme.cardBackground

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSmall
                    spacing: Theme.spacingSmall

                    Text {
                        Layout.fillWidth: true
                        text: modelData.ingredientName || ""
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Text {
                        text: qsTr("%1 %2").arg(modelData.quantity || 0).arg(modelData.unit || "")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                    }

                    Row {
                        spacing: Theme.spacingXSmall
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignVCenter

                        ToolButton {
                            id: deleteBtn
                            implicitWidth: 40
                            implicitHeight: 32
                            text: "删除"
                            font.pointSize: 13
                            flat: true
                            contentItem: Text {
                                text: deleteBtn.text
                                font: deleteBtn.font
                                color: Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: inventoryVM.deleteItem(modelData.id)
                        }

                        ToolButton {
                            id: moreBtn
                            implicitWidth: 40
                            implicitHeight: 32
                            text: "更多"
                            font.pointSize: 13
                            flat: true
                            contentItem: Text {
                                text: moreBtn.text
                                font: moreBtn.font
                                color: Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                currentEditItemId = modelData.id
                                editNameField.text = modelData.ingredientName || ""
                                editQuantityField.text = modelData.quantity !== undefined ? modelData.quantity.toString() : "1"
                                editUnitField.text = modelData.unit || ""
                                editExpiryField.text = modelData.expiryDate || ""
                                moreMenu.popup()
                            }
                        }
                    }

                    BusyIndicator {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        running: inventoryVM.deletingId === modelData.id
                        width: 20
                        height: 20
                        visible: inventoryVM.deletingId === modelData.id
                    }
                }
            }

            footer: Item {
                width: inventoryListView.width - inventoryListView.leftMargin - inventoryListView.rightMargin
                height: 40

                BusyIndicator {
                    anchors.centerIn: parent
                    running: inventoryVM.isLoading && inventoryVM.items.length > 0
                    width: 24
                    height: 24
                }
            }

            onAtYEndChanged: {
                if (atYEnd && !inventoryVM.isLoading && inventoryVM.hasMore) {
                    inventoryVM.loadNextPage()
                }
            }
        }
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("加载中...")
        isLoading: inventoryVM.isLoading && inventoryVM.items.length === 0
    }

    Connections {
        target: inventoryVM
        function onErrorOccurred(error) {
            // 失败兜底：重置提交中标记，避免后续操作被 onItemsChanged 分支误判（成功路径才重置）
            addPending = false
            editPending = false
            if (addDialog.opened) {
                addError.text = error
            } else if (editDialog.opened) {
                editError.text = error
            } else {
                // 页面级提示：守卫拦截（请先登录）已有空态文案「登录后查看你的库存」，不重复显示
                if (error === "请先登录") return
                statusText.text = error
            }
        }
        function onItemsChanged() {
            if (addPending) {
                addPending = false
                itemNameField.clear()
                itemQtyField.clear()
                itemUnitField.clear()
                expiryField.clear()
                addDialog.close()
            }
            if (editPending) {
                editPending = false
                editDialog.close()
            }
        }
    }

    // ===== 更多菜单（下拉到按钮旁边） =====
    Menu {
        id: moreMenu
        modal: true
        dim: true

        background: Rectangle {
            color: Theme.cardBackground
            radius: Theme.radiusMedium
            border.color: Theme.dividerColor
            border.width: 1
        }

        MenuItem {
            id: editMenuItem
            text: qsTr("编辑信息")
            font.pointSize: Theme.fontSizeBody
            contentItem: Label {
                text: editMenuItem.text
                font: editMenuItem.font
                color: Theme.textPrimary
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: editDialog.open()
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitWidth: 200
                implicitHeight: 1
                color: Theme.dividerColor
            }
        }

        MenuItem {
            id: delMenuItem
            text: qsTr("删除")
            font.pointSize: Theme.fontSizeBody
            contentItem: Label {
                text: delMenuItem.text
                font: delMenuItem.font
                color: Theme.errorColor
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: inventoryVM.deleteItem(currentEditItemId)
        }
    }

    // ===== 编辑食材对话框（独立弹窗） =====
    Dialog {
        id: editDialog
        title: qsTr("编辑食材信息")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(parent.width * 0.85, 340)
        onOpened: editError.text = ""

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
                text: qsTr("编辑食材信息")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.bold: true
                color: Theme.textPrimary
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            TextField {
                id: editNameField
                Layout.fillWidth: true
                placeholderText: qsTr("食材名称")
                font.pointSize: Theme.fontSizeBody
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                TextField {
                    id: editQuantityField
                    Layout.preferredWidth: 110
                    placeholderText: qsTr("数量")
                    font.pointSize: Theme.fontSizeBody
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                TextField {
                    id: editUnitField
                    Layout.fillWidth: true
                    placeholderText: qsTr("单位 (默认: 克)")
                    font.pointSize: Theme.fontSizeBody
                }
            }

            TextField {
                id: editExpiryField
                Layout.fillWidth: true
                placeholderText: qsTr("过期日期 (YYYY-MM-DD)")
                font.pointSize: Theme.fontSizeBody
            }

            // ========== 表单校验提示 ==========
            Text {
                id: editError
                Layout.fillWidth: true; height: 16
                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                color: Theme.errorColor; horizontalAlignment: Text.AlignHCenter
                visible: text.length > 0
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("取消")
                    buttonType: CustomButton.ButtonType.Secondary
                    onClicked: editDialog.close()
                }

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("保存")
                    buttonType: CustomButton.ButtonType.Primary
                    enabled: editNameField.text.trim() !== ""
                    onClicked: {
                        if (currentEditItemId === -1) return

                        var qty = parseFloat(editQuantityField.text)
                        if (!qty || qty <= 0) {
                            editError.text = qsTr("数量必须大于 0")
                            return
                        }
                        var unit = editUnitField.text.trim()
                        if (unit.length === 0) unit = "克"   // 单位留空默认克
                        var expiry = editExpiryField.text.trim()
                        if (expiry.length > 0 && !/^\d{4}-\d{1,2}-\d{1,2}$/.test(expiry)) {
                            editError.text = qsTr("过期日期格式应为 YYYY-MM-DD")
                            return
                        }
                        editError.text = ""
                        editPending = true
                        inventoryVM.updateItem(
                            currentEditItemId,
                            editNameField.text.trim(),
                            qty,
                            unit,
                            expiry
                        )
                        // 立即关闭：token 失效时守卫会弹登录页，模态对话框层级高于 StackView，会挡住登录页
                        // （对话框已关闭，其内字段不可见，原有 forceActiveFocus 无效，已移除）
                        editDialog.close()
                    }
                }
            }
        }
    }

    // 添加食材对话框
    Dialog {
        id: addDialog
        title: qsTr("添加食材")
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(parent.width * 0.85, 340)
        // 每次打开清空全部字段：残留的上次输入（尤其过期日期）会随提交打穿服务端日期解析 → 500
        onOpened: {
            itemNameField.text = ""
            itemQtyField.text = ""
            itemUnitField.text = ""
            expiryField.text = ""
            addError.text = ""
        }

        ColumnLayout {
            spacing: Theme.spacingSmall
            width: parent.width

            TextField {
                id: itemNameField
                Layout.fillWidth: true
                placeholderText: qsTr("食材名称 (如: 鸡蛋)")
                font.pointSize: Theme.fontSizeBody
            }

            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: itemQtyField
                    Layout.preferredWidth: 110
                    placeholderText: qsTr("数量")
                    font.pointSize: Theme.fontSizeBody
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                TextField {
                    id: itemUnitField
                    Layout.fillWidth: true
                    placeholderText: qsTr("单位 (默认: 克)")
                    font.pointSize: Theme.fontSizeBody
                }
            }

            TextField {
                id: expiryField
                Layout.fillWidth: true
                placeholderText: qsTr("过期日期 (YYYY-MM-DD)")
                font.pointSize: Theme.fontSizeBody
            }

            // ========== 表单校验提示（红色，按钮上方） ==========
            Text {
                id: addError
                Layout.fillWidth: true; height: 16
                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                color: Theme.errorColor; horizontalAlignment: Text.AlignHCenter
                visible: text.length > 0
            }

            CustomButton {
                Layout.fillWidth: true
                buttonText: qsTr("确定添加")
                enabled: itemNameField.text.trim() !== ""
                onClicked: {
                    // 客户端预校验
                    var qty = parseFloat(itemQtyField.text)
                    if (!qty || qty <= 0) {
                        addError.text = qsTr("数量必须大于 0")
                        return
                    }
                    var unit = itemUnitField.text.trim()
                    if (unit.length === 0) unit = "克"   // 单位留空默认克
                    var expiry = expiryField.text.trim()
                    if (expiry.length > 0 && !/^\d{4}-\d{1,2}-\d{1,2}$/.test(expiry)) {
                        addError.text = qsTr("过期日期格式应为 YYYY-MM-DD")
                        return
                    }
                    addError.text = ""
                    addPending = true
                    inventoryVM.addItem(
                        itemNameField.text.trim(),
                        qty,
                        unit,
                        expiry
                    )
                    // 立即关闭：未登录时守卫会弹登录页（挂起重放），模态对话框层级高于 StackView，会挡住登录页
                    addDialog.close()
                }
            }
        }
    }
}