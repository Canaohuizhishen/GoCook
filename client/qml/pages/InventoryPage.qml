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

    // 单位白名单与校验（服务端 InventoryServiceImpl::validUnits 是唯一事实源，此处为提交前拦截，
    // 两侧清单改动须同步）：空/纯空白提交时默认 "克"（合法）；非空必须命中白名单
    readonly property var supportedUnits: ["个", "克", "千克", "毫升", "升", "只", "条", "把", "根",
                                          "片", "块", "袋", "包", "盒", "瓶", "碗", "勺",
                                          "茶匙", "汤匙", "斤", "两", "磅", "份"]
    function normalizedUnit(raw) { var t = raw.trim(); return t.length === 0 ? "克" : t }
    function isUnitValid(raw) { return supportedUnits.indexOf(normalizedUnit(raw)) !== -1 }

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

        // 过滤框（v2.14）：购物清单按钮下方——按食材名模糊过滤库存，服务端 keyword 过滤（分页同步）
        TextField {
            id: filterField
            // 游客无库存可滤：未登录禁用（输入只会触发需登录守卫，无实际意义）
            enabled: authViewModel.loggedIn
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingMedium
            Layout.rightMargin: Theme.spacingMedium
            placeholderText: qsTr("过滤...")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            color: Theme.textPrimary
            // 给右侧自绘清除按钮留位（Qt Quick Controls 的 TextField 无 clearButtonEnabled，那是 TextArea 的属性）
            rightPadding: 26
            // 输入防抖：停顿 300ms 才提交过滤，避免逐键请求服务端
            onTextChanged: filterDebounce.restart()

            // 自绘清除按钮：非空时显示，点击清空（触发 onTextChanged → 防抖 → setFilterText("") 恢复全量）
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: "×"
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: filterClearMa.pressed ? Theme.primaryColor : Theme.textHint
                visible: filterField.text.length > 0
                MouseArea {
                    id: filterClearMa
                    anchors.fill: parent
                    onClicked: filterField.clear()
                }
            }

            Timer {
                id: filterDebounce
                interval: 300
                onTriggered: inventoryVM.setFilterText(filterField.text)
            }
        }

        // VM 侧过滤词变化（清空按钮/clearAll/登出）同步回显输入框
        Connections {
            target: inventoryVM
            function onFilterTextChanged() {
                if (filterField.text !== inventoryVM.filterText)
                    filterField.text = inventoryVM.filterText
            }
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
            // 空态三态：未登录 → 提示登录；已登录且过滤词非空 → 无匹配提示；否则 → 暂无库存
            text: !authViewModel.loggedIn ? qsTr("登录后查看你的库存")
                : (inventoryVM.filterText !== ""
                    ? qsTr("未找到匹配「%1」的食材").arg(inventoryVM.filterText)
                    : qsTr("暂无库存，点击上方按钮添加食材"))
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

            // 单位预校验提示：与添加框同规则（服务端白名单一致）
            Text {
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.errorColor
                wrapMode: Text.WordWrap
                visible: editNameField.text.trim() !== "" && editUnitField.text.trim().length > 0
                         && !isUnitValid(editUnitField.text)
                text: qsTr("单位不合法，仅支持：%1").arg(supportedUnits.join("/"))
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
                    // 单位非法（非空且不在白名单）→ 按钮置灰不可提交；服务端校验保留为最后防线
                    enabled: editNameField.text.trim() !== "" && isUnitValid(editUnitField.text)
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

            // 单位预校验提示：非法且非空时说明原因（提交按钮已置灰，非法单位发不出请求）
            Text {
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.errorColor
                wrapMode: Text.WordWrap
                visible: itemNameField.text.trim() !== "" && itemUnitField.text.trim().length > 0
                         && !isUnitValid(itemUnitField.text)
                text: qsTr("单位不合法，仅支持：%1").arg(supportedUnits.join("/"))
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
                // 单位非法（非空且不在白名单）→ 按钮置灰不可提交；服务端校验保留为最后防线
                enabled: itemNameField.text.trim() !== "" && isUnitValid(itemUnitField.text)
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