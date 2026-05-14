import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("库存")

    // 当前正在编辑的库存项ID
    property int currentEditItemId: -1

    Component.onCompleted: {
        inventoryVM.loadInventory()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingSmall

        CustomButton {
            id: addButton
            Layout.fillWidth: true
            buttonText: qsTr("+ 添加食材")
            buttonType: CustomButton.ButtonType.Primary
            onClicked: addDialog.open()
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("暂无库存，点击上方按钮添加食材")
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            color: Theme.textHint
            horizontalAlignment: Text.AlignHCenter
            visible: !inventoryVM.isLoading && inventoryVM.items.length === 0
        }

        ListView {
            id: inventoryListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingXSmall
            clip: true
            visible: inventoryVM.items.length > 0

            model: inventoryVM.items

            delegate: Rectangle {
                width: inventoryListView.width
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
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Text {
                        text: qsTr("%1 %2").arg(modelData.quantity || 0).arg(modelData.unit || "")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                    }

                    Row {
                        spacing: Theme.spacingXSmall
                        visible: inventoryVM.deletingId === -1 || inventoryVM.deletingId !== modelData.id

                        ToolButton {
                            id: deleteBtn
                            text: "删除"
                            font.pixelSize: 18
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
                            text: "更多"
                            font.pixelSize: 18
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
                                editExpiryField.text = modelData.expiry || ""
                                moreMenu.popup()
                            }
                        }
                    }

                    BusyIndicator {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        running: inventoryVM.deletingId === modelData.id
                        width: 20
                        height: 20
                        visible: inventoryVM.deletingId === modelData.id
                    }
                }
            }

            footer: Item {
                width: inventoryListView.width
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
            console.log("Inventory error:", error)
        }
    }

    // ===== 更多菜单（下拉到按钮旁边） =====
    Menu {
        id: moreMenu
        modal: true
        dim: false

        background: Rectangle {
            color: Theme.cardBackground
            radius: Theme.radiusMedium
            border.color: Theme.dividerColor
            border.width: 1
        }

        MenuItem {
            id: editMenuItem
            text: qsTr("编辑信息")
            font.pixelSize: Theme.fontSizeBody
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
            font.pixelSize: Theme.fontSizeBody
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
        width: 280

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
                font.pixelSize: Theme.fontSizeH3
                font.bold: true
                color: Theme.textPrimary
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            TextField {
                id: editNameField
                Layout.fillWidth: true
                placeholderText: qsTr("食材名称")
                font.pixelSize: Theme.fontSizeBody
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                TextField {
                    id: editQuantityField
                    Layout.preferredWidth: 110
                    placeholderText: qsTr("数量")
                    font.pixelSize: Theme.fontSizeBody
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                TextField {
                    id: editUnitField
                    Layout.fillWidth: true
                    placeholderText: qsTr("单位")
                    font.pixelSize: Theme.fontSizeBody
                }
            }

            TextField {
                id: editExpiryField
                Layout.fillWidth: true
                placeholderText: qsTr("过期日期 (YYYY-MM-DD)")
                font.pixelSize: Theme.fontSizeBody
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
                        if (currentEditItemId !== -1) {
                            inventoryVM.updateItem(
                                currentEditItemId,
                                editNameField.text.trim(),
                                parseFloat(editQuantityField.text) || 0,
                                editUnitField.text.trim() || qsTr("个"),
                                editExpiryField.text.trim()
                            )
                        }
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

        ColumnLayout {
            spacing: Theme.spacingSmall
            width: 280

            TextField {
                id: itemNameField
                Layout.fillWidth: true
                placeholderText: qsTr("食材名称 (如: 鸡蛋)")
                font.pixelSize: Theme.fontSizeBody
            }

            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: itemQtyField
                    Layout.preferredWidth: 110
                    placeholderText: qsTr("数量")
                    font.pixelSize: Theme.fontSizeBody
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                TextField {
                    id: itemUnitField
                    Layout.fillWidth: true
                    placeholderText: qsTr("单位 (如: 个)")
                    font.pixelSize: Theme.fontSizeBody
                }
            }

            TextField {
                id: expiryField
                Layout.fillWidth: true
                placeholderText: qsTr("过期日期 (YYYY-MM-DD)")
                font.pixelSize: Theme.fontSizeBody
            }

            CustomButton {
                Layout.fillWidth: true
                buttonText: qsTr("确定添加")
                enabled: itemNameField.text.trim() !== ""
                onClicked: {
                    inventoryVM.addItem(
                        itemNameField.text.trim(),
                        parseFloat(itemQtyField.text) || 1,
                        itemUnitField.text.trim() || qsTr("个"),
                        expiryField.text.trim()
                    )
                    itemNameField.clear()
                    itemQtyField.clear()
                    itemUnitField.clear()
                    expiryField.clear()
                    addDialog.close()
                }
            }
        }
    }
}