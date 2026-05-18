import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    title: qsTr("提交菜谱")

    property bool submitting: false

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
                text: qsTr("提交菜谱")
                font.pointSize: Theme.fontSizeBody
                font.weight: Theme.fontWeightMedium
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
            }
            Item { Layout.preferredWidth: 44 }
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        contentWidth: width
        contentHeight: formColumn.implicitHeight + Theme.spacingLarge
        clip: true

        Column {
            id: formColumn
            width: parent.width
            spacing: Theme.spacingMedium

            Text {
                width: parent.width
                text: qsTr("基本信息")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }

            TextField {
                id: nameField
                width: parent.width
                placeholderText: qsTr("菜谱名称 *")
                font.pointSize: Theme.fontSizeBody
            }

            TextField {
                id: imageUrlField
                width: parent.width
                placeholderText: qsTr("封面图片 URL")
                font.pointSize: Theme.fontSizeBody
            }

            TextArea {
                id: descField
                width: parent.width
                height: 80
                placeholderText: qsTr("描述")
                font.pointSize: Theme.fontSizeBody
                wrapMode: TextArea.WordWrap
            }

            Rectangle { width: parent.width; height: 1; color: Theme.dividerColor }

            Text {
                width: parent.width
                text: qsTr("食材")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }

            ListView {
                id: ingredientsList
                width: parent.width
                height: contentHeight + (ingredientListModel.count > 0 ? 48 : 0)
                interactive: false
                model: ListModel { id: ingredientListModel }
                spacing: 4
                delegate: Rectangle {
                    width: ingredientsList.width
                    height: 44
                    radius: Theme.radiusSmall
                    color: Theme.cardBackground
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("%1. %2 %3 %4").arg(index + 1).arg(name).arg(quantity).arg(unit)
                            font.pointSize: Theme.fontSizeBody
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Button {
                            text: qsTr("删除")
                            flat: true
                            font.pointSize: Theme.fontSizeSmall
                            onClicked: ingredientListModel.remove(index)
                        }
                    }
                }
                footer: CustomButton {
                    width: ingredientsList.width
                    buttonText: qsTr("+ 添加食材")
                    buttonType: CustomButton.ButtonType.Secondary
                    onClicked: ingredientDialog.open()
                }
            }

            Rectangle { width: parent.width; height: 1; color: Theme.dividerColor }

            Text {
                width: parent.width
                text: qsTr("步骤")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }

            ListView {
                id: stepsList
                width: parent.width
                height: contentHeight + (stepListModel.count > 0 ? 48 : 0)
                interactive: false
                model: ListModel { id: stepListModel }
                spacing: 4
                delegate: Rectangle {
                    width: stepsList.width
                    height: 44
                    radius: Theme.radiusSmall
                    color: Theme.cardBackground
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("步骤%1: %2").arg(order).arg(description)
                            font.pointSize: Theme.fontSizeBody
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Button {
                            text: qsTr("删除")
                            flat: true
                            font.pointSize: Theme.fontSizeSmall
                            onClicked: stepListModel.remove(index)
                        }
                    }
                }
                footer: CustomButton {
                    width: stepsList.width
                    buttonText: qsTr("+ 添加步骤")
                    buttonType: CustomButton.ButtonType.Secondary
                    onClicked: stepDialog.open()
                }
            }

            Rectangle { width: parent.width; height: 1; color: Theme.dividerColor }

            CustomButton {
                width: parent.width
                buttonText: submitting ? qsTr("提交中...") : qsTr("提交菜谱")
                enabled: nameField.text.trim() !== "" && !submitting
                        && ingredientListModel.count > 0 && stepListModel.count > 0
                onClicked: {
                    submitting = true
                    var ingredients = []
                    var steps = []
                    for (var i = 0; i < ingredientListModel.count; i++) {
                        var ing = ingredientListModel.get(i)
                        ingredients.push({name: ing.name, quantity: ing.quantity, unit: ing.unit})
                    }
                    for (var j = 0; j < stepListModel.count; j++) {
                        var s = stepListModel.get(j)
                        steps.push({order: s.order, description: s.description})
                    }
                    recipeVM.submitRecipe(
                        nameField.text.trim(),
                        descField.text.trim(),
                        imageUrlField.text.trim(),
                        ingredients,
                        steps,
                        []
                    )
                }
            }

            Item { height: Theme.spacingLarge }
        }
    }

    Dialog {
        id: ingredientDialog
        title: qsTr("添加食材")
        anchors.centerIn: parent
        modal: true
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

            TextField { id: ingNameField; placeholderText: qsTr("食材名"); Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: ingQtyField; placeholderText: qsTr("数量"); Layout.preferredWidth: 100; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                TextField { id: ingUnitField; placeholderText: qsTr("单位 (如 克)"); Layout.fillWidth: true }
            }
            CustomButton {
                Layout.fillWidth: true
                buttonText: qsTr("确定")
                enabled: ingNameField.text.trim() !== ""
                onClicked: {
                    ingredientListModel.append({
                        name: ingNameField.text.trim(),
                        quantity: parseFloat(ingQtyField.text) || 0,
                        unit: ingUnitField.text.trim() || qsTr("克")
                    })
                    ingNameField.clear()
                    ingQtyField.clear()
                    ingUnitField.clear()
                    ingredientDialog.close()
                }
            }
        }
    }

    Dialog {
        id: stepDialog
        title: qsTr("添加步骤")
        anchors.centerIn: parent
        modal: true
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
            TextField { id: stepDescField; placeholderText: qsTr("步骤描述"); Layout.fillWidth: true }
            CustomButton {
                Layout.fillWidth: true
                buttonText: qsTr("确定")
                enabled: stepDescField.text.trim() !== ""
                onClicked: {
                    stepListModel.append({
                        order: stepListModel.count + 1,
                        description: stepDescField.text.trim()
                    })
                    stepDescField.clear()
                    stepDialog.close()
                }
            }
        }
    }

    Connections {
        target: recipeVM
        function onRecipeSubmitted(id, status) {
            submitting = false
            snackBar.show(qsTr("提交成功！ID: %1").arg(id))
            popTimer.start()
        }
        function onSubmitFailed(error) {
            submitting = false
            snackBar.show(qsTr("提交失败: %1").arg(error))
        }
    }

    Timer {
        id: popTimer
        interval: 1500
        onTriggered: _stackView.pop()
    }

    Rectangle {
        id: snackBar
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: Theme.spacingLarge
        width: Math.min(parent.width * 0.9, 350)
        height: msgText.implicitHeight + Theme.spacingMedium
        radius: Theme.radiusMedium
        color: Theme.textPrimary
        opacity: 0
        visible: opacity > 0

        function show(msg) {
            msgText.text = msg
            opacity = 0.9
        }

        Text {
            id: msgText
            anchors.centerIn: parent
            color: "white"
            font.pointSize: Theme.fontSizeCaption
        }

        Behavior on opacity { NumberAnimation { duration: 300 } }
    }
}
