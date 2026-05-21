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

    property real maxNameWidth: 80

    // 用于测量最长的食材名宽度的隐藏 Text 元素
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
            var maxW = 60
            for (var i = 0; i < items.length; i++) {
                nameMeasurer.text = items[i].ingredientName || ""
                var w = nameMeasurer.implicitWidth
                if (w > maxW) maxW = w
            }
            root.maxNameWidth = maxW + 8
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
        }

        // 用于测量最长的食材名宽度的隐藏 Text 元素
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
                        Text { width: 65; height: implicitHeight
                            text: qsTr("需购")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: 65; height: implicitHeight
                            text: qsTr("库存")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: 70; height: implicitHeight
                            text: qsTr("建议买")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: 60; height: implicitHeight
                            text: qsTr("单位")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.bold: true
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text { width: 60; height: implicitHeight
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
                            Text { width: 65; height: implicitHeight
                                text: modelData.requiredQuantity || 0
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: 65; height: implicitHeight
                                text: modelData.inventoryQuantity || 0
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: 70; height: implicitHeight
                                text: modelData.toBuyQuantity || 0
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                font.bold: true
                                color: modelData.toBuyQuantity > 0 ? Theme.primaryColor : Theme.textHint
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: 60; height: implicitHeight
                                text: modelData.unit || ""
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text { width: 60; height: implicitHeight
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

    // 错误提示
    Connections {
        target: shoppingListVM
        function onErrorOccurred(error) {
            console.log("ShoppingList detail error:", error)
        }
    }
}
