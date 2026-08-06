import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: favoritesPage
    title: qsTr("我的收藏")

    signal showDetailRequest(int recipeId)

    property string currentGroupFilter: ""
    property bool showCreateDialog: false
    property bool __dataLoaded: false
    property bool editMode: false
    property var selectedIds: []
    property int moveFavId: 0
    property string errorMessage: ""

    // QML 侧缓存的展示列表 — 从 recipeVM.favorites 按 currentGroupFilter 即时过滤
    property var displayFavorites: []

    function updateDisplayFavorites() {
        var all = recipeVM.favorites
        if (currentGroupFilter === "") {
            displayFavorites = all
        } else {
            var filtered = []
            for (var i = 0; i < all.length; i++) {
                if (all[i].groupName === currentGroupFilter)
                    filtered.push(all[i])
            }
            displayFavorites = filtered
        }
    }

    onCurrentGroupFilterChanged: updateDisplayFavorites()

    // 每次页面可见时重新加载数据（用户从其他 tab 切回来时刷新）
    onVisibleChanged: {
        if (visible) {
            errorMessage = ""
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
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                editMode = !editMode
                if (!editMode) selectedIds = []
            }
        }
    }

    // 收藏写操作失败提示（底部 toast；主请求失败由离线视图呈现，此处不重复提示）
    ErrorBanner {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 100
        anchors.horizontalCenter: parent.horizontalCenter
        text: errorMessage
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
                                    color: currentGroupFilter === "" ? Theme.primaryColor
                                         : parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.25)
                                         : parent.hovered ? Qt.rgba(0, 0, 0, 0.06)
                                         : Theme.searchBarBackground
                                    Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
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
                                        color: currentGroupFilter === modelData.name ? Theme.primaryColor
                                             : parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.25)
                                             : parent.hovered ? Qt.rgba(0, 0, 0, 0.06)
                                             : Theme.searchBarBackground
                                        Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
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
                    isLoading: recipeVM.favoritesLoading && displayFavorites.length === 0
                }

                // 加载失败且无旧数据 → 居中离线视图（PDD 式）；有旧数据则静默显示旧数据
                NetworkOfflineView {
                    anchors.fill: parent
                    active: recipeVM.favoritesLoadFailed && !recipeVM.favoritesLoading && displayFavorites.length === 0
                    onRetryRequested: {
                        recipeVM.loadFavorites(1, 20, currentGroupFilter)
                        recipeVM.loadFavoriteGroups()
                    }
                }

                ListView {
                    id: favoritesListView
                    anchors.fill: parent
                    leftMargin: Theme.spacingMedium
                    rightMargin: Theme.spacingMedium
                    spacing: Theme.spacingMedium
                    clip: true
                    visible: displayFavorites.length > 0

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    model: displayFavorites

                    delegate: Rectangle {
                        width: favoritesListView.width - favoritesListView.leftMargin - favoritesListView.rightMargin
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
                                    cache: false
                                    source: modelData.imageUrl ? authViewModel.apiBaseUrl + modelData.imageUrl : ""
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
                                        color: parent.down ? Theme.primaryDarkColor
                                             : parent.hovered ? Theme.primaryLightColor
                                             : Theme.primaryColor
                                        opacity: 0.9
                                        Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
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
                                        color: parent.down ? "#C62828"
                                             : parent.hovered ? "#EF5350"
                                             : "#E74C3C"
                                        opacity: 0.9
                                        Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
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
                    visible: displayFavorites.length === 0 && !recipeVM.favoritesLoading && !recipeVM.favoritesLoadFailed

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
        visible: editMode && displayFavorites.length > 0
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
                text: selectedIds.length === displayFavorites.length
                      ? qsTr("取消全选") : qsTr("全选")
                flat: true
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 80
                implicitHeight: 34
                leftPadding: 0; rightPadding: 0
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.down ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.2)
                         : parent.hovered ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.08)
                         : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: Theme.primaryColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (selectedIds.length === displayFavorites.length) {
                        selectedIds = []
                    } else {
                        var all = []
                        for (var i = 0; i < displayFavorites.length; i++)
                            all.push(displayFavorites[i].id)
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
                    color: !parent.enabled ? Theme.searchBarBackground
                         : parent.down ? Theme.primaryDarkColor
                         : parent.hovered ? Theme.primaryLightColor
                         : Theme.primaryColor
                    border.color: !parent.enabled ? Theme.dividerColor : "transparent"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
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
                    color: !parent.enabled ? Theme.searchBarBackground
                         : parent.down ? "#C62828"
                         : parent.hovered ? "#EF5350"
                         : "#E74C3C"
                    border.color: !parent.enabled ? Theme.dividerColor : "transparent"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: Theme.durationShort; easing.type: Easing.OutCubic } }
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
    ConfirmDialog {
        id: batchDeleteDialog
        dialogTitle: qsTr("确认删除")
        message: qsTr("确定要删除选中的 %1 个收藏吗？").arg(selectedIds.length)
        confirmText: qsTr("删除")
        confirmColor: Theme.errorColor
        onConfirmed: {
            recipeVM.batchRemoveFavorites(selectedIds)
            editMode = false
            selectedIds = []
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

    SimpleListInputDialog {
        id: createGroupDialog
        dialogTitle: qsTr("创建新分组")
        placeholderText: qsTr("输入分组名称")
        confirmText: qsTr("创建")
        onConfirmed: function(text) {
            recipeVM.createFavoriteGroup(text)
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
            enabled: groupContextMenu.groupId > 0
            onClicked: {
                renameGroupDialog.open()
            }
        }

        MenuItem {
            text: qsTr("删除分组")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            enabled: groupContextMenu.groupId > 0
            onClicked: {
                recipeVM.deleteFavoriteGroup(groupContextMenu.groupId)
                currentGroupFilter = "默认收藏夹"
                // 模拟点击默认收藏夹标签：加载该分组数据（只查 group_id IS NULL，安全无竞态）
                recipeVM.loadFavorites(1, 20, "默认收藏夹")
            }
        }
    }

    SimpleListInputDialog {
        id: renameGroupDialog
        dialogTitle: qsTr("重命名分组")
        placeholderText: qsTr("输入新名称")
        confirmText: qsTr("保存")
        inputText: groupContextMenu.groupName
        onConfirmed: function(text) {
            recipeVM.updateFavoriteGroupName(groupContextMenu.groupId, text)
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
        function onFavoritesChanged() {
            updateDisplayFavorites()
        }
        function onFavoriteMoved() {
            recipeVM.loadFavorites(1, 20, currentGroupFilter)
            recipeVM.loadFavoriteGroups()
        }
        function onFavoriteRemoved() {
            recipeVM.loadFavorites(1, 20, currentGroupFilter)
            recipeVM.loadFavoriteGroups()
        }
        function onFavoriteGroupDeleted() {
            recipeVM.loadFavorites(1, 20, currentGroupFilter)
            recipeVM.loadFavoriteGroups()
        }
        function onFavoriteOperationFailed(error) {
            // 先清空再赋值：同文案连续错误也能重启自动消失计时（ErrorBanner 约定）
            errorMessage = ""
            errorMessage = error
        }
    }
}
