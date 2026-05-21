import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: favoritesPage
    title: qsTr("我的收藏")

    background: Rectangle { color: Theme.backgroundColor }

    signal showDetailRequest(int recipeId)

    property string currentGroupFilter: ""
    property bool showCreateDialog: false
    property bool __dataLoaded: false
    property string errorMessage: ""
    property bool editMode: false
    property var selectedIds: []
    property int moveFavId: 0

    // 错误提示自动消失
    Timer {
        id: errorTimer
        interval: 3000
        onTriggered: errorMessage = ""
    }

    // 每次页面可见时重新加载数据（用户从其他 tab 切回来时刷新）
    onVisibleChanged: {
        if (visible) {
            recipeVM.loadFavorites(1, 20, currentGroupFilter)
            recipeVM.loadFavoriteGroups()
            __dataLoaded = true
        } else {
            editMode = false
            selectedIds = []
        }
    }

    // 页面首次创建时：如果 SwipeView 直接以可见状态创建（首次切换到该 tab），
    // onVisibleChanged 并不会触发，需要手动加载数据
    Component.onCompleted: {
        if (visible) {
            recipeVM.loadFavorites(1, 20, currentGroupFilter)
            recipeVM.loadFavoriteGroups()
            __dataLoaded = true
        }
    }

    // ========== 顶部 ==========
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 48
        color: "transparent"

        Label {
            anchors.centerIn: parent
            text: qsTr("我的收藏")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH2
            font.weight: Theme.fontWeightBold
            color: Theme.textPrimary
        }

        // 编辑/完成按钮
        Button {
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.verticalCenter: parent.verticalCenter
            text: editMode ? qsTr("完成") : qsTr("编辑")
            flat: true
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: Theme.primaryColor
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                editMode = !editMode
                if (!editMode) selectedIds = []
            }
        }
    }

    // 错误提示
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 48
        width: parent.width
        height: 32
        color: "#E74C3C"
        visible: errorMessage.length > 0

        Text {
            anchors.centerIn: parent
            text: errorMessage
            color: "white"
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeCaption
        }
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: 48
        anchors.bottomMargin: editMode ? 52 : 0

        Column {
            anchors.fill: parent

            // ========== 分组栏（横向滚动） ==========
            Rectangle {
                width: parent.width
                height: 44
                color: Theme.cardBackground
                border.color: Theme.dividerColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingSmall
                    anchors.rightMargin: Theme.spacingSmall
                    spacing: Theme.spacingXSmall

                    // 横向可滚动的分组标签
                    Flickable {
                        id: groupsFlickable
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: row.width
                        contentHeight: groupsFlickable.height
                        flickableDirection: Flickable.HorizontalFlick
                        interactive: true
                        clip: true

                        Row {
                            id: row
                            y: Math.max(0, (groupsFlickable.height - height) / 2)
                            spacing: Theme.spacingXSmall

                            // "全部" 按钮
                            Button {
                                height: 30
                                text: qsTr("全部") + " (" + recipeVM.favoritesTotalCount + ")"
                                flat: true
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeCaption
                                leftPadding: 10; rightPadding: 10
                                topPadding: 0; bottomPadding: 0

                                background: Rectangle {
                                    radius: Theme.radiusSmall
                                    color: currentGroupFilter === "" ? Theme.primaryColor : Theme.searchBarBackground
                                }
                                contentItem: Text {
                                    text: parent.text
                                    font: parent.font
                                    color: currentGroupFilter === "" ? "white" : Theme.textPrimary
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    currentGroupFilter = ""
                                    recipeVM.loadFavorites(1, 20, "")
                                }
                            }

                            Repeater {
                                model: recipeVM.favoriteGroups

                                Button {
                                    id: groupBtn
                                    height: 30
                                    text: modelData.name + " (" + modelData.count + ")"
                                    flat: true
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    leftPadding: 10; rightPadding: 10
                                    topPadding: 0; bottomPadding: 0

                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: currentGroupFilter === modelData.name ? Theme.primaryColor : Theme.searchBarBackground
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        font: parent.font
                                        color: currentGroupFilter === modelData.name ? "white" : Theme.textPrimary
                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }

                                    // 左键点击 → 筛选该分组
                                    onClicked: {
                                        currentGroupFilter = modelData.name
                                        recipeVM.loadFavorites(1, 20, modelData.name)
                                    }

                                    // 右键点击 → 分组菜单（重命名/删除）
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        // 使用 onPressed 避免 Button 事件拦截导致的延迟
                                        onPressed: function(mouse) {
                                            mouse.accepted = true
                                            groupContextMenu.groupId = modelData.id
                                            groupContextMenu.groupName = modelData.name
                                            groupContextMenu.popup(mouse.x + groupBtn.x, mouse.y + groupBtn.y)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 创建分组按钮
                    ToolButton {
                        implicitWidth: 36; implicitHeight: 36
                        flat: true
                        contentItem: Text {
                            text: "+"
                            font.pointSize: 22
                            color: Theme.primaryColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: { createGroupDialog.open() }
                    }
                }
            }

            // ========== 收藏列表 ==========
            Rectangle {
                width: parent.width
                height: parent.height - 44
                color: Theme.backgroundColor

                LoadingIndicator {
                    id: loadingIndicator
                    fullscreen: true
                    message: qsTr("正在加载收藏...")
                    isLoading: recipeVM.favoritesLoading && recipeVM.favorites.length === 0
                }

                ListView {
                    id: favoritesListView
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    clip: true
                    visible: recipeVM.favorites.length > 0

                    model: recipeVM.favorites

                    delegate: Rectangle {
                        width: favoritesListView.width
                        height: 110
                        radius: Theme.radiusMedium
                        color: Theme.cardBackground
                        border.color: Theme.dividerColor
                        border.width: 1

                        // 必须先声明 MouseArea（在下层），再声明 RowLayout（在上层），
                        // 这样按钮才能接收到点击事件，不会被 MouseArea 吞掉
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (editMode) {
                                    var idx = selectedIds.indexOf(modelData.id)
                                    if (idx >= 0)
                                        selectedIds = selectedIds.slice(0, idx).concat(selectedIds.slice(idx + 1))
                                    else
                                        selectedIds = selectedIds.concat([modelData.id])
                                } else {
                                    favoritesPage.showDetailRequest(modelData.recipeId)
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMedium
                            spacing: Theme.spacingMedium

                            // 缩略图
                            Rectangle {
                                Layout.preferredWidth: 70; Layout.preferredHeight: 70
                                radius: Theme.radiusSmall
                                color: Theme.searchBarBackground
                                Image {
                                    anchors.fill: parent
                                    source: modelData.imageUrl || ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    visible: status === Image.Ready
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "🍽"
                                    font.pointSize: 24
                                    visible: parent.children[0].status !== Image.Ready
                                }
                            }

                            // 文字信息
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 4

                                Text {
                                    text: modelData.name
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    font.weight: Theme.fontWeightMedium
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: modelData.description || ""
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    color: Theme.textHint
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    visible: text.length > 0
                                }

                                Text {
                                    text: modelData.groupName || qsTr("默认收藏夹")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    color: Theme.accentColor
                                }
                            }

                            // 操作按钮
                            ColumnLayout {
                                Layout.alignment: Qt.AlignVCenter
                                spacing: Theme.spacingXSmall

                                // 普通模式：移动分组按钮
                                Button {
                                    visible: !editMode
                                    text: qsTr("移动分组")
                                    flat: true
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    leftPadding: 8; rightPadding: 8
                                    topPadding: 4; bottomPadding: 4
                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: Theme.primaryColor
                                        opacity: 0.9
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        font: parent.font
                                        color: "white"
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    onClicked: {
                                        moveFavId = modelData.id
                                        moveFavGroupDialog.open()
                                    }
                                }

                                // 普通模式：取消收藏按钮
                                Button {
                                    visible: !editMode
                                    text: qsTr("取消收藏")
                                    flat: true
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeCaption
                                    leftPadding: 8; rightPadding: 8
                                    topPadding: 4; bottomPadding: 4
                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: "#E74C3C"
                                        opacity: 0.9
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        font: parent.font
                                        color: "white"
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    onClicked: {
                                        recipeVM.removeFavorite(modelData.id)
                                    }
                                }

                                // 编辑模式：复选框
                                CheckBox {
                                    visible: editMode
                                    Layout.alignment: Qt.AlignHCenter
                                    checked: selectedIds.indexOf(modelData.id) >= 0
                                    onClicked: {
                                        var idx = selectedIds.indexOf(modelData.id)
                                        if (idx >= 0)
                                            selectedIds = selectedIds.slice(0, idx).concat(selectedIds.slice(idx + 1))
                                        else
                                            selectedIds = selectedIds.concat([modelData.id])
                                    }
                                }
                            }
                        }
                    }

                    onAtYEndChanged: {
                        if (atYEnd && !recipeVM.favoritesLoading && recipeVM.favoritesHasMore)
                            recipeVM.loadMoreFavorites()
                    }
                }

                // ========== 空状态 ==========
                Column {
                    anchors.centerIn: parent
                    spacing: Theme.spacingMedium
                    visible: recipeVM.favorites.length === 0 && !recipeVM.favoritesLoading

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "\u2606"
                        font.pointSize: 48
                        color: Theme.textHint
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("收藏喜欢的菜谱")
                        color: Theme.textHint; font.pointSize: Theme.fontSizeBody
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("在菜谱详情页点击收藏")
                        color: Theme.textHint; font.pointSize: Theme.fontSizeCaption; opacity: 0.6
                    }
                }
            }
        }
    }

    // ========== 批量删除底部操作栏 ==========
    Rectangle {
        visible: editMode && recipeVM.favorites.length > 0
        anchors.bottom: parent.bottom
        width: parent.width
        height: 52
        color: Theme.cardBackground
        border.color: Theme.dividerColor
        border.width: 1
        z: 20

        RowLayout {
            anchors.fill: parent
            anchors.topMargin: 0; anchors.bottomMargin: 0
            anchors.leftMargin: Theme.spacingSmall
            anchors.rightMargin: Theme.spacingSmall
            spacing: 4

            Text {
                text: qsTr("已选 %1 项").arg(selectedIds.length)
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textSecondary
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }

            Button {
                text: selectedIds.length === recipeVM.favorites.length
                      ? qsTr("取消全选") : qsTr("全选")
                flat: true
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 80
                implicitHeight: 34
                leftPadding: 0; rightPadding: 0
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.primaryColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (selectedIds.length === recipeVM.favorites.length) {
                        selectedIds = []
                    } else {
                        var all = []
                        for (var i = 0; i < recipeVM.favorites.length; i++)
                            all.push(recipeVM.favorites[i].id)
                        selectedIds = all
                    }
                }
            }

            Button {
                enabled: selectedIds.length > 0
                text: qsTr("移动")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                implicitHeight: 30
                leftPadding: 10; rightPadding: 10
                Layout.alignment: Qt.AlignVCenter
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.enabled ? Theme.primaryColor : Theme.searchBarBackground
                    border.color: parent.enabled ? "transparent" : Theme.dividerColor
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.enabled ? "white" : Theme.textHint
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    moveFavId = 0
                    moveFavGroupDialog.open()
                }
            }

            Button {
                enabled: selectedIds.length > 0
                text: qsTr("删除")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                implicitHeight: 30
                leftPadding: 10; rightPadding: 10
                Layout.alignment: Qt.AlignVCenter
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.enabled ? "#E74C3C" : Theme.searchBarBackground
                    border.color: parent.enabled ? "transparent" : Theme.dividerColor
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.enabled ? "white" : Theme.textHint
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: batchDeleteDialog.open()
            }
        }
    }

    // ========== 批量删除确认弹窗 ==========
    Dialog {
        id: batchDeleteDialog
        modal: true
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: Math.min(parent.width * 0.8, 320)

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.cardBackground
            border.color: Theme.dividerColor
        }

        Column {
            spacing: Theme.spacingMedium
            width: parent.width

            Text {
                text: qsTr("确认删除")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightBold
                color: Theme.textPrimary
            }

            Text {
                text: qsTr("确定要删除选中的 %1 个收藏吗？").arg(selectedIds.length)
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            RowLayout {
                width: parent.width
                spacing: Theme.spacingMedium

                Button {
                    Layout.fillWidth: true; height: 40
                    text: qsTr("取消")
                    flat: true
                    background: Rectangle {
                        radius: Theme.radiusMedium
                        color: "transparent"
                        border.color: Theme.dividerColor
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text; font: parent.font
                        color: Theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: batchDeleteDialog.close()
                }

                Button {
                    Layout.fillWidth: true; height: 40
                    text: qsTr("删除")
                    background: Rectangle { radius: Theme.radiusMedium; color: "#E74C3C" }
                    contentItem: Text {
                        text: parent.text; font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        recipeVM.batchRemoveFavorites(selectedIds)
                        batchDeleteDialog.close()
                        editMode = false
                        selectedIds = []
                    }
                }
            }
        }
    }

    // ========== 移动收藏到分组弹窗（单条/批量共用） ==========
    Dialog {
        id: moveFavGroupDialog
        modal: true; standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape
        x: (parent.width - width) / 2; y: (parent.height - height) / 2
        width: Math.min(parent.width * 0.8, 320)

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.cardBackground
            border.color: Theme.dividerColor
        }

        Column {
            width: parent.width
            spacing: Theme.spacingMedium
            topPadding: Theme.spacingMedium
            bottomPadding: Theme.spacingMedium

            Text {
                text: qsTr("选择目标分组")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightBold
                color: Theme.textPrimary
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Item { width: 1; height: 1 }

            Repeater {
                id: moveGroupRepeater
                width: parent.width
                model: recipeVM.favoriteGroups

                Rectangle {
                    width: parent.width
                    height: 44
                    radius: Theme.radiusSmall
                    color: moveGroupHovered ? Theme.searchBarBackground : "transparent"
                    property bool moveGroupHovered: false

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.spacingMedium
                        text: (moveFavId > 0 ? "\u2606 " : "") + (modelData.name || "") + " (" + (modelData.count || 0) + ")"
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: parent.moveGroupHovered = true
                        onExited: parent.moveGroupHovered = false
                        onClicked: {
                            if (moveFavId > 0) {
                                // 单条移动
                                recipeVM.moveFavorite(moveFavId, modelData.id)
                                moveFavId = 0
                            } else {
                                // 批量移动
                                recipeVM.batchMoveFavorites(selectedIds, modelData.id)
                                selectedIds = []
                                editMode = false
                            }
                            moveFavGroupDialog.close()
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.dividerColor
                        opacity: 0.3
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 44
                radius: Theme.radiusMedium
                color: "transparent"
                border.color: Theme.dividerColor
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: qsTr("取消")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.textSecondary
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: { moveFavId = 0; moveFavGroupDialog.close() }
                }
            }
        }
    }

    // ========== 创建分组弹窗 ==========
    Dialog {
        id: createGroupDialog
        modal: true; standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape
        x: (parent.width - width) / 2; y: (parent.height - height) / 2
        width: Math.min(parent.width * 0.8, 320)

        Column {
            spacing: Theme.spacingMedium
            width: parent.width

            Text {
                text: qsTr("创建新分组")
                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightBold; color: Theme.textPrimary
            }

            TextField {
                id: groupNameInput
                width: parent.width; height: 44
                placeholderText: qsTr("输入分组名称")
                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                verticalAlignment: TextInput.AlignVCenter
                background: Rectangle {
                    radius: Theme.radiusMedium
                    color: Theme.cardBackground; border.color: Theme.dividerColor
                }
            }

            RowLayout {
                width: parent.width
                spacing: Theme.spacingMedium
                Button {
                    Layout.fillWidth: true; height: 40
                    text: qsTr("取消")
                    flat: true
                    background: Rectangle {
                        radius: Theme.radiusMedium
                        color: "transparent"; border.color: Theme.dividerColor; border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text; font: parent.font
                        color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: { createGroupDialog.close(); groupNameInput.text = "" }
                }
                Button {
                    Layout.fillWidth: true; height: 40
                    text: qsTr("创建")
                    background: Rectangle { radius: Theme.radiusMedium; color: Theme.primaryColor }
                    contentItem: Text {
                        text: parent.text; font: parent.font
                        color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (groupNameInput.text.trim().length > 0) {
                            recipeVM.createFavoriteGroup(groupNameInput.text.trim())
                            createGroupDialog.close()
                            groupNameInput.text = ""
                        }
                    }
                }
            }
        }
    }

    // ========== 右键分组菜单 ==========
    Menu {
        id: groupContextMenu
        property int groupId: 0
        property string groupName: ""

        MenuItem {
            text: qsTr("更新名称")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            onClicked: {
                renameGroupInput.text = groupContextMenu.groupName
                renameGroupDialog.open()
            }
        }

        MenuItem {
            text: qsTr("删除分组")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            onClicked: {
                recipeVM.deleteFavoriteGroup(groupContextMenu.groupId)
            }
        }
    }

    // ========== 重命名分组弹窗 ==========
    Dialog {
        id: renameGroupDialog
        modal: true; standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape
        x: (parent.width - width) / 2; y: (parent.height - height) / 2
        width: Math.min(parent.width * 0.8, 320)

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.cardBackground
            border.color: Theme.dividerColor
        }

        Column {
            spacing: Theme.spacingMedium
            width: parent.width

            Text {
                text: qsTr("重命名分组")
                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightBold; color: Theme.textPrimary
            }

            TextField {
                id: renameGroupInput
                width: parent.width; height: 44
                placeholderText: qsTr("输入新名称")
                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                verticalAlignment: TextInput.AlignVCenter
                background: Rectangle {
                    radius: Theme.radiusMedium
                    color: Theme.cardBackground; border.color: Theme.dividerColor
                }
            }

            RowLayout {
                width: parent.width
                spacing: Theme.spacingMedium

                Button {
                    Layout.fillWidth: true; height: 40
                    text: qsTr("取消")
                    flat: true
                    background: Rectangle {
                        radius: Theme.radiusMedium
                        color: "transparent"; border.color: Theme.dividerColor; border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text; font: parent.font
                        color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: { renameGroupDialog.close() }
                }

                Button {
                    Layout.fillWidth: true; height: 40
                    text: qsTr("保存")
                    background: Rectangle { radius: Theme.radiusMedium; color: Theme.primaryColor }
                    contentItem: Text {
                        text: parent.text; font: parent.font
                        color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        var newName = renameGroupInput.text.trim()
                        if (newName.length > 0) {
                            recipeVM.updateFavoriteGroupName(groupContextMenu.groupId, newName)
                            renameGroupDialog.close()
                        }
                    }
                }
            }
        }
    }

    // ========== 登录完成后自动加载收藏数据 ==========
    Connections {
        target: authViewModel
        function onLoggedInChanged() {
            if (authViewModel.loggedIn) {
                recipeVM.loadFavorites(1, 20, currentGroupFilter)
                recipeVM.loadFavoriteGroups()
            }
        }
    }

    Connections {
        target: recipeVM
        function onErrorOccurred(error) { console.log("Favorites error:", error) }
        function onFavoriteOperationFailed(error) {
            errorMessage = error
            errorTimer.restart()
        }
    }
}
