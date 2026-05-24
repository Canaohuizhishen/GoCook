import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: healthProfilePage
    title: qsTr("健康指标")

    // ========== 导航 ==========
    function goBack() {
        var item = healthProfilePage.parent
        while (item) {
            try { if (typeof item.pop === "function") { item.pop(); return } } catch(e) {}
            item = item.parent
        }
        var sv = StackView.view
        if (sv && typeof sv.pop === "function") sv.pop()
    }

    // ========== 数据 ==========
    property int    heightValue: 0
    property double weightValue: 0.0
    property var    selectedConditions: []
    property var    avoidances: []
    property bool   showEdit: false       // false=浏览, true=编辑
    property bool   hasData: false        // 服务端是否有已保存数据

    readonly property var conditionOptions: ["高血压", "高血脂", "糖尿病", "胃炎", "痛风", "无"]
    readonly property color colorCondition: "#E74C3C"

    // ========== 加载数据 ==========
    Component.onCompleted: {
        authViewModel.loadHealthProfile()
        showEdit = false
    }

    // ========== 顶部标题 ==========
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 48
        color: "transparent"

        Label {
            anchors.centerIn: parent
            text: showEdit ? qsTr("编辑健康指标") : qsTr("健康指标")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH2
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }

        ToolButton {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: 44; implicitHeight: 44
            flat: true
            contentItem: Canvas {
                width: 24; height: 24
                property color arrowColor: Theme.textPrimary
                onArrowColorChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.strokeStyle = arrowColor
                    ctx.lineWidth = 2; ctx.lineCap = "round"; ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(16, 6); ctx.lineTo(8, 12); ctx.lineTo(16, 18)
                    ctx.stroke()
                }
            }
            onClicked: {
                if (showEdit)
                    showEdit = false
                else
                    healthProfilePage.goBack()
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: 48

        Flickable {
            anchors.fill: parent
            contentWidth: width
            contentHeight: outerColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            ColumnLayout {
                id: outerColumn
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingMedium
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacingMedium
                spacing: Theme.spacingLarge

                // ====================================================
                // 浏览模式 — 显示已保存的指标
                // ====================================================
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    visible: !showEdit

                    // ---------- 说明 ----------
                    Text {
                        Layout.fillWidth: true
                        text: hasData ? qsTr("您的健康指标如下") : qsTr("暂未设置健康指标")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.textHint
                    }

                    // ---------- 身体指标卡片 ----------
                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radiusMedium
                        color: Theme.cardBackground
                        border.color: Theme.dividerColor
                        border.width: 1
                        visible: hasData
                        height: 80

                        Row {
                            anchors.centerIn: parent
                            spacing: Theme.spacingXLarge

                            // 身高
                            Column {
                                spacing: 4
                                Text {
                                    text: qsTr("身高")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    color: Theme.textHint
                                }
                                Text {
                                    text: heightValue > 0 ? heightValue + " cm" : qsTr("未设置")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeH2
                                    font.weight: Theme.fontWeightBold
                                    color: Theme.textPrimary
                                }
                            }

                            Rectangle {
                                width: 1; height: 40
                                color: Theme.dividerColor
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // 体重
                            Column {
                                spacing: 4
                                Text {
                                    text: qsTr("体重")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    color: Theme.textHint
                                }
                                Text {
                                    text: weightValue > 0 ? weightValue + " kg" : qsTr("未设置")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeH2
                                    font.weight: Theme.fontWeightBold
                                    color: Theme.textPrimary
                                }
                            }

                            Rectangle {
                                width: 1; height: 40
                                color: Theme.dividerColor
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // BMI (由服务端自动计算，客户端仅展示)
                            Column {
                                spacing: 4
                                Text {
                                    text: qsTr("BMI")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    color: Theme.textHint
                                }
                                Text {
                                    text: {
                                        if (heightValue > 0 && weightValue > 0) {
                                            var h = heightValue / 100.0
                                            var bmi = (weightValue / (h * h)).toFixed(1)
                                            return bmi
                                        }
                                        return qsTr("—")
                                    }
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeH2
                                    font.weight: Theme.fontWeightBold
                                    color: Theme.textPrimary
                                }
                            }
                        }
                    }

                    // ---------- 健康问题 ----------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall
                        visible: hasData

                        RowLayout {
                            spacing: Theme.spacingXSmall
                            Layout.fillWidth: true

                            Rectangle {
                                width: 4; height: 16; radius: 2
                                color: colorCondition
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Text {
                                text: qsTr("健康问题")
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeH3
                                font.weight: Theme.fontWeightMedium
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXSmall
                            visible: selectedConditions.length > 0
                            Repeater {
                                model: selectedConditions
                                Rectangle {
                                    height: 30
                                    width: txtCond.implicitWidth + Theme.spacingMedium * 2
                                    radius: Theme.radiusSmall
                                    color: colorCondition
                                    Text {
                                        id: txtCond
                                        anchors.centerIn: parent
                                        text: modelData
                                        font.family: Theme.fontFamily
                                        font.pointSize: Theme.fontSizeCaption
                                        font.weight: Theme.fontWeightMedium
                                        color: "white"
                                    }
                                }
                            }
                        }

                        Text {
                            text: qsTr("无")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            color: Theme.textHint
                            opacity: 0.5
                            visible: selectedConditions.length === 0
                        }
                    }

                    // ---------- 忌口建议 ----------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall
                        visible: avoidances.length > 0

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.dividerColor }

                        RowLayout {
                            spacing: Theme.spacingXSmall
                            Layout.fillWidth: true
                            Rectangle {
                                width: 4; height: 16; radius: 2
                                color: colorCondition
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Text {
                                text: qsTr("饮食建议")
                                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeH3
                                font.weight: Theme.fontWeightMedium; color: Theme.textPrimary
                                Layout.fillWidth: true
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: Theme.spacingSmall
                            rowSpacing: Theme.spacingSmall

                            Repeater {
                                model: avoidances

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: childrenRect.height + Theme.spacingMedium * 2
                                    radius: Theme.radiusSmall
                                    color: Theme.cardBackground; border.color: Theme.dividerColor; border.width: 1
                                    Column {
                                        x: Theme.spacingMedium; y: Theme.spacingMedium
                                        width: parent.width - Theme.spacingMedium * 2
                                        spacing: Theme.spacingXSmall
                                        Text {
                                            text: qsTr("• ") + modelData.ingredient
                                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeBody
                                            font.weight: Theme.fontWeightMedium; color: colorCondition
                                            width: parent.width; wrapMode: Text.WordWrap
                                        }
                                        Text {
                                            text: modelData.reason
                                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                                            color: Theme.textSecondary; wrapMode: Text.WordWrap; width: parent.width
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ---------- 编辑按钮 ----------
                    CustomButton {
                        Layout.fillWidth: true
                        buttonText: qsTr("编辑健康指标")
                        buttonType: CustomButton.ButtonType.Primary
                        onClicked: showEdit = true
                    }

                    // ---------- 返回按钮 ----------
                    CustomButton {
                        Layout.fillWidth: true
                        buttonText: qsTr("返回")
                        buttonType: CustomButton.ButtonType.Secondary
                        onClicked: healthProfilePage.goBack()
                    }

                    Item { Layout.fillWidth: true; implicitHeight: Theme.spacingXLarge }
                }

                // ====================================================
                // 编辑模式 — 修改健康指标
                // ====================================================
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    visible: showEdit

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("填写您的身体指标和健康情况，我们将据此为您推荐更合适的饮食方案。")
                        font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                        color: Theme.textHint; wrapMode: Text.WordWrap
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.dividerColor }

                    // ---------- 身高 ----------
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: Theme.spacingXSmall
                        Text {
                            text: qsTr("身高（厘米）")
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint
                        }
                        Rectangle {
                            Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                            color: Theme.cardBackground; border.color: Theme.dividerColor
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                                spacing: Theme.spacingXSmall
                                TextInput {
                                    id: heightInput
                                    Layout.fillWidth: true; verticalAlignment: TextInput.AlignVCenter
                                    font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                                    inputMethodHints: Qt.ImhDigitsOnly; selectByMouse: true
                                    text: healthProfilePage.heightValue > 0 ? healthProfilePage.heightValue : ""
                                    onTextChanged: {
                                        var val = parseInt(text)
                                        healthProfilePage.heightValue = isNaN(val) ? 0 : val
                                    }
                                }
                                Text { text: qsTr("cm"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                            }
                        }
                    }

                    // ---------- 体重 ----------
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: Theme.spacingXSmall
                        Text {
                            text: qsTr("体重（公斤）")
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint
                        }
                        Rectangle {
                            Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                            color: Theme.cardBackground; border.color: Theme.dividerColor
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                                spacing: Theme.spacingXSmall
                                TextInput {
                                    id: weightInput
                                    Layout.fillWidth: true; verticalAlignment: TextInput.AlignVCenter
                                    font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly; selectByMouse: true
                                    text: healthProfilePage.weightValue > 0 ? healthProfilePage.weightValue : ""
                                    onTextChanged: {
                                        var val = parseFloat(text)
                                        healthProfilePage.weightValue = isNaN(val) ? 0.0 : val
                                    }
                                }
                                Text { text: qsTr("kg"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                            }
                        }
                    }

                    // ---------- 健康问题 ----------
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: Theme.spacingXSmall
                        Text {
                            text: qsTr("健康问题（点击选择，可多选）")
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXSmall
                            Repeater {
                                model: conditionOptions
                                Button {
                                    height: 34; text: conditionOptions[index]; flat: true
                                    font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                                    leftPadding: 12; rightPadding: 12; topPadding: 0; bottomPadding: 0
                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: selectedConditions.indexOf(conditionOptions[index]) >= 0 ? colorCondition
                                             : parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.2)
                                             : parent.hovered ? Qt.rgba(0, 0, 0, 0.06)
                                             : Theme.searchBarBackground
                                        border.color: selectedConditions.indexOf(conditionOptions[index]) >= 0 ? colorCondition
                                                    : parent.down ? Theme.primaryColor
                                                    : parent.hovered ? Theme.primaryLightColor
                                                    : Theme.dividerColor
                                        border.width: 1
                                        Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                                        Behavior on border.color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                                    }
                                    contentItem: Text {
                                        text: conditionOptions[index]; font: parent.font
                                        color: selectedConditions.indexOf(conditionOptions[index]) >= 0 ? "white" : Theme.textPrimary
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        var data = conditionOptions[index]
                                        if (data === "无") { selectedConditions = ["无"]; return }
                                        var idx = selectedConditions.indexOf("无")
                                        if (idx >= 0) {
                                            var tmp = selectedConditions; tmp.splice(idx, 1); selectedConditions = tmp
                                        }
                                        if (selectedConditions.indexOf(data) >= 0)
                                            selectedConditions = selectedConditions.filter(function(x) { return x !== data })
                                        else
                                            selectedConditions = selectedConditions.concat([data])
                                        if (selectedConditions.length === 0)
                                            selectedConditions = ["无"]
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.dividerColor }

                    // ---------- 状态提示 ----------
                    Text {
                        id: statusText; Layout.fillWidth: true; height: 20
                        font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                        color: Theme.accentColor; horizontalAlignment: Text.AlignHCenter
                        visible: text.length > 0
                    }

                    // ---------- 保存按钮 ----------
                    CustomButton {
                        Layout.fillWidth: true
                        buttonText: qsTr("保存健康指标")
                        buttonType: CustomButton.ButtonType.Primary
                        onClicked: {
                            if (healthProfilePage.heightValue <= 0 || healthProfilePage.weightValue <= 0) {
                                statusText.text = qsTr("请输入身高和体重"); return
                            }
                            statusText.text = qsTr("正在保存...")
                            var conditions = selectedConditions.filter(function(x) { return x !== "无" })
                            authViewModel.saveHealthProfile(healthProfilePage.heightValue,
                                                            healthProfilePage.weightValue, conditions)
                        }
                    }

                    // ---------- 取消按钮 ----------
                    CustomButton {
                        Layout.fillWidth: true
                        buttonText: qsTr("取消")
                        buttonType: CustomButton.ButtonType.Secondary
                        onClicked: {
                            showEdit = false
                            statusText.text = ""
                        }
                    }

                    Item { Layout.fillWidth: true; implicitHeight: Theme.spacingXLarge }
                }
            }
        }
    }

    Connections {
        target: authViewModel
        function onHealthProfileLoaded(heightCm, weightKg, conditions, avoidances) {
            healthProfilePage.heightValue = heightCm
            healthProfilePage.weightValue = weightKg
            healthProfilePage.selectedConditions = conditions.length > 0 ? conditions : ["无"]
            healthProfilePage.avoidances = avoidances
            healthProfilePage.hasData = heightCm > 0 || weightKg > 0 || conditions.length > 0
        }
        function onHealthProfileLoadFailed(error) {
            statusText.text = qsTr("加载失败: ") + error
        }
        function onHealthProfileSaved(avoidances) {
            statusText.text = qsTr("保存完成")
            healthProfilePage.avoidances = avoidances
            healthProfilePage.hasData = true
            showEdit = false
            // Reload to get updated data from server (height, weight, conditions)
            authViewModel.loadHealthProfile()
        }
        function onHealthProfileSaveFailed(error) {
            statusText.text = qsTr("保存失败: ") + error
        }
    }
}
