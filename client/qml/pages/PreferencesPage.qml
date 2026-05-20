import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: preferencesPage
    title: qsTr("饮食偏好设置")

    // ========== 导航 ==========
    function goBack() {
        var item = preferencesPage.parent
        while (item) {
            try { if (typeof item.pop === "function") { item.pop(); return } } catch(e) {}
            item = item.parent
        }
        var sv = StackView.view
        if (sv && typeof sv.pop === "function") sv.pop()
    }

    // ========== 偏好数据 ==========
    property var   prefLikes:      []
    property var   prefDislikes:   []
    property string prefHealthGoal: ""
    property bool   showEdit:      false    // false = 浏览模式, true = 编辑模式

    // ========== 可选项 ==========
    readonly property var likeOptions:       ["中式", "西式", "日式", "韩式", "清淡", "麻辣", "甜", "酸", "鲜", "香辣", "东南亚"]
    readonly property var dislikeOptions:   ["香菜", "花生", "海鲜", "牛奶", "鸡蛋", "麸质", "大豆", "坚果", "芹菜", "味精"]
    readonly property var healthGoalOptions: ["减脂", "增肌", "保持健康", "无"]

    // ========== 三种类别颜色 ==========
    readonly property color colorLikes:      Theme.primaryColor       // 橙 — 偏好口味
    readonly property color colorDislikes:   Theme.accentColor        // 绿 — 饮食禁忌
    readonly property color colorHealthGoal: "#5B9BD5"               // 蓝 — 健康目标

    // ========== 加载偏好 ==========
    Component.onCompleted: {
        authViewModel.loadPreferences()
        showEdit = false   // 默认进入浏览模式
    }

    // ========== 顶部标题 ==========
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 48
        color: "transparent"

        Label {
            anchors.centerIn: parent
            text: showEdit ? qsTr("编辑饮食偏好") : qsTr("我的饮食偏好")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH2
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }

        ToolButton {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: 44
            implicitHeight: 44
            flat: true
            contentItem: Canvas {
                width: 24
                height: 24
                property color arrowColor: Theme.textPrimary
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.strokeStyle = arrowColor
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(16, 6)
                    ctx.lineTo(8, 12)
                    ctx.lineTo(16, 18)
                    ctx.stroke()
                }
            }
            onClicked: {
                if (showEdit) {
                    showEdit = false
                } else {
                    preferencesPage.goBack()
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: 48

        ScrollView {
            anchors.fill: parent; clip: true; contentWidth: availableWidth
            Column {
                id: outerColumn
                width: Math.min(parent.width - Theme.spacingLarge * 2, 400)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top; anchors.topMargin: Theme.spacingMedium
                spacing: Theme.spacingLarge

                // ====================================================
                // 浏览模式 — 显示三个分类（有标签显示标签，无标签显示"未设置"）
                // ====================================================
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.spacingLarge
                    visible: !showEdit

                    // ---------- 偏好口味/菜系 ----------
                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacingSmall

                        RowLayout {
                            spacing: Theme.spacingXSmall
                            Layout.fillWidth: true

                            Rectangle {
                                width: 4; height: 16; radius: 2
                                color: colorLikes
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: qsTr("偏好口味/菜系")
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeH3
                                font.weight: Theme.fontWeightMedium
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }
                        }

                        Flow {
                            spacing: Theme.spacingXSmall
                            width: parent.width
                            visible: prefLikes.length > 0

                            Repeater {
                                model: prefLikes

                                Rectangle {
                                    height: 30
                                    width: txt.implicitWidth + Theme.spacingMedium * 2
                                    radius: Theme.radiusSmall
                                    color: colorLikes

                                    Text {
                                        id: txt
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
                            text: qsTr("未设置")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            color: Theme.textHint
                            opacity: 0.5
                            visible: prefLikes.length === 0
                        }
                    }

                    // ---------- 饮食禁忌 ----------
                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacingSmall

                        RowLayout {
                            spacing: Theme.spacingXSmall
                            Layout.fillWidth: true

                            Rectangle {
                                width: 4; height: 16; radius: 2
                                color: colorDislikes
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: qsTr("饮食禁忌")
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeH3
                                font.weight: Theme.fontWeightMedium
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }
                        }

                        Flow {
                            spacing: Theme.spacingXSmall
                            width: parent.width
                            visible: prefDislikes.length > 0

                            Repeater {
                                model: prefDislikes

                                Rectangle {
                                    height: 30
                                    width: txt2.implicitWidth + Theme.spacingMedium * 2
                                    radius: Theme.radiusSmall
                                    color: colorDislikes

                                    Text {
                                        id: txt2
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
                            text: qsTr("未设置")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            color: Theme.textHint
                            opacity: 0.5
                            visible: prefDislikes.length === 0
                        }
                    }

                    // ---------- 健康目标 ----------
                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacingSmall

                        RowLayout {
                            spacing: Theme.spacingXSmall
                            Layout.fillWidth: true

                            Rectangle {
                                width: 4; height: 16; radius: 2
                                color: colorHealthGoal
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: qsTr("健康目标")
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeH3
                                font.weight: Theme.fontWeightMedium
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }
                        }

                        Rectangle {
                            height: 30
                            width: goalTxt.implicitWidth + Theme.spacingMedium * 2
                            radius: Theme.radiusSmall
                            color: colorHealthGoal
                            visible: prefHealthGoal !== "" && prefHealthGoal !== "无"

                            Text {
                                id: goalTxt
                                anchors.centerIn: parent
                                text: prefHealthGoal
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeCaption
                                font.weight: Theme.fontWeightMedium
                                color: "white"
                            }
                        }

                        Text {
                            text: qsTr("未设置")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            color: Theme.textHint
                            opacity: 0.5
                            visible: prefHealthGoal === "" || prefHealthGoal === "无"
                        }
                    }

                    // ---------- 分隔线 ----------
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.dividerColor
                    }

                    // ---------- 更改按钮 ----------
                    Button {
                        width: parent.width; height: 50
                        text: qsTr("更改饮食偏好")
                        background: Rectangle {
                            radius: Theme.radiusMedium
                            color: Theme.primaryColor
                        }
                        contentItem: Text {
                            text: parent.text
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody
                            font.weight: Theme.fontWeightMedium
                            color: Theme.textOnPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            showEdit = true
                        }
                    }

                    // ---------- 返回按钮 ----------
                    Button {
                        width: parent.width; height: 50
                        text: qsTr("返回")
                        background: Rectangle {
                            radius: Theme.radiusMedium; color: "transparent"
                            border.color: Theme.primaryColor; border.width: 1
                        }
                        contentItem: Text {
                            text: parent.text
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody
                            color: Theme.primaryColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: preferencesPage.goBack()
                    }

                    Item { width: 1; height: Theme.spacingXLarge }
                }

                // ====================================================
                // 编辑模式 — 可选标签
                // ====================================================
                Column {
                    width: parent.width
                    spacing: Theme.spacingLarge
                    visible: showEdit

                    // ---------- 说明文字 ----------
                    Text {
                        width: parent.width
                        text: qsTr("设置您的口味偏好、饮食禁忌和健康目标，我们将据此为您推荐更合适的菜谱。")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.textHint
                        wrapMode: Text.WordWrap
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.dividerColor }

                    // ---------- 偏好口味/菜系（点击多选） ----------
                    Column {
                        width: parent.width
                        spacing: Theme.spacingXSmall

                        Text {
                            text: qsTr("偏好口味/菜系（点击选择，可多选）")
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint
                        }

                        Flow {
                            spacing: Theme.spacingXSmall
                            width: outerColumn.width

                            Repeater {
                                model: likeOptions

                                Button {
                                    height: 34
                                    text: likeOptions[index]
                                    flat: true
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    leftPadding: 12
                                    rightPadding: 12
                                    topPadding: 0
                                    bottomPadding: 0

                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: prefLikes.indexOf(likeOptions[index]) >= 0 ? colorLikes : Theme.searchBarBackground
                                        border.color: prefLikes.indexOf(likeOptions[index]) >= 0 ? colorLikes : Theme.dividerColor
                                        border.width: 1
                                    }

                                    contentItem: Text {
                                        text: likeOptions[index]
                                        font: parent.font
                                        color: prefLikes.indexOf(likeOptions[index]) >= 0 ? "white" : Theme.textPrimary
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        var data = likeOptions[index]
                                        if (prefLikes.indexOf(data) >= 0)
                                            prefLikes = prefLikes.filter(function(x) { return x !== data })
                                        else
                                            prefLikes = prefLikes.concat([data])
                                    }
                                }
                            }
                        }
                    }

                    // ---------- 饮食禁忌（点击多选） ----------
                    Column {
                        width: parent.width
                        spacing: Theme.spacingXSmall

                        Text {
                            text: qsTr("饮食禁忌（点击选择，可多选）")
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint
                        }

                        Flow {
                            spacing: Theme.spacingXSmall
                            width: outerColumn.width

                            Repeater {
                                model: dislikeOptions

                                Button {
                                    height: 34
                                    text: dislikeOptions[index]
                                    flat: true
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    leftPadding: 12
                                    rightPadding: 12
                                    topPadding: 0
                                    bottomPadding: 0

                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: prefDislikes.indexOf(dislikeOptions[index]) >= 0 ? colorDislikes : Theme.searchBarBackground
                                        border.color: prefDislikes.indexOf(dislikeOptions[index]) >= 0 ? colorDislikes : Theme.dividerColor
                                        border.width: 1
                                    }

                                    contentItem: Text {
                                        text: dislikeOptions[index]
                                        font: parent.font
                                        color: prefDislikes.indexOf(dislikeOptions[index]) >= 0 ? "white" : Theme.textPrimary
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        var data = dislikeOptions[index]
                                        if (prefDislikes.indexOf(data) >= 0)
                                            prefDislikes = prefDislikes.filter(function(x) { return x !== data })
                                        else
                                            prefDislikes = prefDislikes.concat([data])
                                    }
                                }
                            }
                        }
                    }

                    // ---------- 健康目标（单选） ----------
                    Column {
                        width: parent.width
                        spacing: Theme.spacingXSmall

                        Text {
                            text: qsTr("健康目标")
                            font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint
                        }

                        Flow {
                            spacing: Theme.spacingXSmall
                            width: outerColumn.width

                            Repeater {
                                model: healthGoalOptions

                                Button {
                                    height: 34
                                    text: healthGoalOptions[index]
                                    flat: true
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    leftPadding: 12
                                    rightPadding: 12
                                    topPadding: 0
                                    bottomPadding: 0

                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: prefHealthGoal === healthGoalOptions[index] ? colorHealthGoal : Theme.searchBarBackground
                                        border.color: prefHealthGoal === healthGoalOptions[index] ? colorHealthGoal : Theme.dividerColor
                                        border.width: 1
                                    }

                                    contentItem: Text {
                                        text: healthGoalOptions[index]
                                        font: parent.font
                                        color: prefHealthGoal === healthGoalOptions[index] ? "white" : Theme.textPrimary
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        prefHealthGoal = healthGoalOptions[index]
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.dividerColor }

                    // ========== 状态提示 ==========
                    Text {
                        id: statusText; width: parent.width; height: 20
                        font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                        color: Theme.accentColor; horizontalAlignment: Text.AlignHCenter
                        visible: text.length > 0
                    }

                    // ========== 保存按钮 ==========
                    Button {
                        id: saveBtn; width: parent.width; height: 50
                        text: qsTr("保存偏好设置")
                        background: Rectangle { radius: Theme.radiusMedium; color: Theme.primaryColor }
                        contentItem: Text {
                            text: saveBtn.text; font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium
                            color: Theme.textOnPrimary; horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            statusText.text = qsTr("正在保存...")
                            authViewModel.savePreferences(prefLikes, prefDislikes, prefHealthGoal)
                        }
                    }

                    // ========== 取消按钮 ==========
                    Button {
                        id: cancelBtn; width: parent.width; height: 50
                        text: qsTr("取消")
                        background: Rectangle {
                            radius: Theme.radiusMedium; color: "transparent"
                            border.color: Theme.primaryColor; border.width: 1
                        }
                        contentItem: Text {
                            text: cancelBtn.text; font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody; color: Theme.primaryColor
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            showEdit = false
                            statusText.text = ""
                        }
                    }

                    Item { width: 1; height: Theme.spacingXLarge }
                }
            }
        }
    }

    Connections {
        target: authViewModel
        function onPreferencesLoaded(likes, dislikes, healthGoal) {
            prefLikes = likes
            prefDislikes = dislikes
            prefHealthGoal = healthGoal
        }
        function onPreferencesLoadFailed(error) {
            statusText.text = qsTr("加载失败: ") + error
        }
        function onPreferencesSaved() {
            statusText.text = qsTr("保存完成")
            showEdit = false
        }
        function onPreferencesSaveFailed(error) {
            statusText.text = qsTr("保存失败: ") + error
        }
    }
}
