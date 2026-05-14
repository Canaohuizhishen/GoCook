import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("库存")

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

                    ToolButton {
                        text: "删除"
                        font {
                            family: "Apple Color Emoji, Segoe UI Emoji, Noto Color Emoji, sans-serif"
                            pixelSize: 18
                        }
                        onClicked: inventoryVM.deleteItem(modelData.id)
                    }

                    BusyIndicator {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: Theme.spacingSmall
                        running: inventoryVM.deletingId === modelData.id
                        width: 20
                        height: 20
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
