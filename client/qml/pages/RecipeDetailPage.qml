import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    title: qsTr("菜谱详情")

    property int recipeId: 0
    property bool isFavorited: false
    property bool videosAttempted: false
    property bool ratingsTriggered: false
    property int newRating: 0
    property string newComment: ""
    property int editRatingId: 0
    property int editRatingValue: 0
    property string editComment: ""
    readonly property var recipeTags: recipeVM.recipeDetail.tags || []
    readonly property var nutrition: recipeVM.recipeDetail.nutrition || {}
    readonly property real imageHeight: (flickable.width - Theme.spacingMedium * 2) * 0.5
    readonly property real navThreshold: imageHeight - navBar.height

    Component.onCompleted: {
        if (recipeId > 0)
            recipeVM.loadRecipeDetail(recipeId)
    }

    LoadingIndicator {
        id: loadingIndicator
        fullscreen: true
        message: qsTr("正在加载菜谱...")
        isLoading: recipeVM.detailLoading
    }

    // 无缓存 + 断网/加载失败 → 居中离线视图（PDD 式；有缓存时静默显示缓存，本视图不出现）
    NetworkOfflineView {
        id: offlineView
        anchors.fill: parent
        active: recipeVM.detailLoadFailed && !recipeVM.detailLoading
        // 低于浮动导航栏（navBar z:10）：离线时返回按钮仍可点
        z: 5
        onRetryRequested: {
            if (recipeId > 0)
                recipeVM.loadRecipeDetail(recipeId)
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        // 加载中/加载失败时隐藏内容：页面保持空白 + 居中转圈（LoadingIndicator），
        // 失败后由 NetworkOfflineView 覆盖；缓存命中/加载成功后正常显示
        visible: !recipeVM.detailLoading && !recipeVM.detailLoadFailed
        contentWidth: width
        contentHeight: detailColumn.implicitHeight + Theme.spacingLarge + navBar.height + bottomBar.height * 2 + Theme.spacingMedium
        clip: true
        topMargin: -navBar.height
        bottomMargin: -bottomBar.height

        Column {
            id: detailColumn
            width: parent.width - Theme.spacingMedium * 2
            x: Theme.spacingMedium
            y: navBar.height + Theme.spacingMedium
            spacing: Theme.spacingMedium

            Rectangle {
                width: parent.width
                height: imageHeight
                radius: Theme.radiusMedium
                color: Theme.dividerColor
                clip: true

                Image {
                    id: detailImage
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    cache: false
                    source: recipeVM.recipeDetail.imageUrl ? authViewModel.apiBaseUrl + recipeVM.recipeDetail.imageUrl : "qrc:/img-placeholder.svg"
                    asynchronous: true
                }
            }

            Text {
                id: detailName
                width: parent.width
                text: recipeVM.recipeDetail.name || qsTr("加载中...")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH2
                font.weight: Theme.fontWeightBold
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }

            Flow {
                width: parent.width
                spacing: Theme.spacingSmall
                visible: recipeTags != null && recipeTags.length > 0

                Repeater {
                    model: recipeTags
                    TagChip {
                        text: modelData
                    }
                }
            }

            Row {
                width: parent.width
                spacing: Theme.spacingMedium

                Row {
                    spacing: 4
                    Canvas {
                        width: 14
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter
                        property color iconColor: Theme.textSecondary
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1
                            ctx.beginPath()
                            ctx.arc(7, 7, 5.5, 0, Math.PI * 2)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(7, 7)
                            ctx.lineTo(7, 3.5)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(7, 7)
                            ctx.lineTo(10, 7)
                            ctx.stroke()
                        }
                    }
                    Text {
                        text: qsTr("准备 %1分钟").arg(recipeVM.recipeDetail.prepTime || 0)
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                    }
                }
                Row {
                    spacing: 4
                    Canvas {
                        width: 14
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter
                        property color iconColor: Theme.textSecondary
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1
                            ctx.beginPath()
                            ctx.moveTo(7, 1.5)
                            ctx.quadraticCurveTo(13, 5, 7, 12)
                            ctx.quadraticCurveTo(1, 5, 7, 1.5)
                            ctx.stroke()
                        }
                    }
                    Text {
                        text: qsTr("烹饪 %1分钟").arg(recipeVM.recipeDetail.cookTime || 0)
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                    }
                }
                Text {
                    text: "|"
                    color: Theme.dividerColor
                }
                Text {
                    text: qsTr("作者: ") + (recipeVM.recipeDetail.authorName || "")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeCaption
                    color: Theme.textSecondary
                }
            }

            Text {
                width: parent.width
                text: recipeVM.recipeDetail.description || ""
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeBody
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                visible: text !== ""
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
            }

            SectionHeader {
                headerText: qsTr("食材")
            }

            Text {
                id: noIngredientsText
                width: parent.width
                text: qsTr("(暂无食材)")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textHint
                visible: true
            }

            Column {
                id: ingredientsColumn
                width: parent.width
                spacing: 8

                Repeater {
                    id: ingredientsRepeater
                    model: 0
                    delegate: RecipeIngredientItem {
                        width: ingredientsColumn.width
                        name: modelData.name
                        quantity: modelData.quantity
                        unit: modelData.unit
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
            }

            SectionHeader {
                headerText: qsTr("步骤")
            }

            Text {
                id: noStepsText
                width: parent.width
                text: qsTr("(暂无步骤)")
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textHint
                visible: true
            }

            Column {
                id: stepsColumn
                width: parent.width
                spacing: Theme.spacingMedium

                Repeater {
                    id: stepsRepeater
                    model: 0
                    delegate: RecipeStepItem {
                        width: stepsColumn.width
                        stepNumber: modelData.order || index + 1
                        description: modelData.description
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
                // 与 NutritionSummaryCard 的有数据判定一致（has_data 缺省时回退 calories>0）
                visible: nutrition && (nutrition.has_data !== undefined
                                       ? nutrition.has_data : nutrition.calories > 0)
            }

            NutritionSummaryCard {
                detailRecipeId: recipeId
                stackView: _stackView
            }

            // ---- 关联视频区域 ----
            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
                visible: recipeVM.recipeVideos.length > 0 || recipeVM.videosLoading
            }

            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                SectionHeader {
                    headerText: qsTr("关联视频")
                    visible: recipeVM.recipeVideos.length > 0
                }

                // 加载按钮（首次点击触发加载）
                CustomButton {
                    width: parent.width
                    buttonText: "\u25B6 " + qsTr("查看关联视频")
                    buttonType: CustomButton.ButtonType.Secondary
                    visible: recipeVM.recipeVideos.length === 0 && !recipeVM.videosLoading
                    onClicked: {
                        videosAttempted = true
                        recipeVM.loadRecipeVideos(recipeId)
                    }
                }

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: recipeVM.videosLoading
                    width: 40
                    height: 40
                    palette.dark: Theme.primaryColor
                    visible: recipeVM.videosLoading
                }

                // 视频卡片列表
                Repeater {
                    id: videosRepeater
                    model: 0
                    delegate: VideoCardDelegate {
                        title: modelData.title || ""
                        imageUrl: (modelData && modelData.image_url) ? authViewModel.apiBaseUrl + modelData.image_url : ""
                        platform: modelData.platform || ""
                        durationSeconds: modelData.duration_seconds || 0
                        onClicked: {
                            if (modelData && modelData.url)
                                Qt.openUrlExternally(modelData.url)
                        }
                    }
                }

                // 空状态提示
                Text {
                    width: parent.width
                    text: qsTr("该菜谱暂无关联视频")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.textHint
                    horizontalAlignment: Text.AlignHCenter
                    visible: recipeVM.recipeVideos.length === 0 && !recipeVM.videosLoading && videosAttempted
                }
            }

            // ---- 评分与评论区域 ----
            Rectangle {
                width: parent.width
                height: 1
                color: Theme.dividerColor
                visible: ratingsTriggered
            }

            Column {
                width: parent.width
                spacing: Theme.spacingSmall

                SectionHeader {
                    headerText: qsTr("评分与评论")
                    visible: ratingsTriggered && recipeVM.recipeRatings.length > 0
                }

                // 加载按钮（首次点击触发）
                CustomButton {
                    width: parent.width
                    buttonText: "\u2605 " + qsTr("查看评分与评论")
                    buttonType: CustomButton.ButtonType.Secondary
                    visible: !ratingsTriggered && !recipeVM.ratingsLoading
                    onClicked: {
                        ratingsTriggered = true
                        recipeVM.loadRecipeRatings(recipeId)
                        recipeVM.loadMyRecipeRating(recipeId)
                    }
                }

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: recipeVM.ratingsLoading
                    width: 40
                    height: 40
                    palette.dark: Theme.primaryColor
                    visible: recipeVM.ratingsLoading
                }

                // 本人的评分置顶卡片（已评分时显示）
                Rectangle {
                    width: parent.width
                    height: myRatingCardContent.implicitHeight + Theme.spacingSmall * 2
                    color: Theme.cardBackground
                    radius: Theme.radiusMedium
                    border.width: 1
                    border.color: Theme.primaryColor
                    visible: ratingsTriggered && recipeVM.myRating && recipeVM.myRating.id > 0

                    Column {
                        id: myRatingCardContent
                        width: parent.width - Theme.spacingSmall * 2
                        anchors.centerIn: parent
                        spacing: Theme.spacingXSmall

                        Text {
                            width: parent.width
                            text: qsTr("我的评论")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.weight: Theme.fontWeightBold
                            color: Theme.primaryColor
                        }

                        RowLayout {
                            width: parent.width

                            Text {
                                text: recipeVM.myRating.username || ""
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                font.weight: Theme.fontWeightMedium
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }

                            Text {
                                text: {
                                    var stars = ""
                                    var r = recipeVM.myRating.rating || 0
                                    for (var i = 1; i <= 5; i++)
                                        stars += (i <= r ? "\u2605" : "\u2606")
                                    return stars
                                }
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.warningColor
                            }
                        }

                        Text {
                            width: parent.width
                            text: recipeVM.myRating.comment || ""
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                            visible: (recipeVM.myRating.comment || "").length > 0
                        }

                        Text {
                            width: parent.width
                            text: recipeVM.myRating.createdAt || ""
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeSmall
                            color: Theme.textHint
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                // 编辑模式卡片
                Rectangle {
                    width: parent.width
                    height: editFormColumn.implicitHeight + Theme.spacingMedium * 2
                    color: Theme.cardBackground
                    radius: Theme.radiusMedium
                    border.width: 1
                    border.color: Theme.primaryColor
                    visible: ratingsTriggered && recipeVM.myRating && recipeVM.myRating.id > 0 && editRatingId === recipeVM.myRating.id

                    Column {
                        id: editFormColumn
                        width: parent.width - Theme.spacingMedium * 2
                        anchors.centerIn: parent
                        spacing: Theme.spacingSmall

                        // 标题
                        Text {
                            text: "\u270E " + qsTr("编辑我的评分")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeCaption
                            font.weight: Theme.fontWeightBold
                            color: Theme.primaryColor
                        }

                        // 分隔线
                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Theme.dividerColor
                            opacity: 0.3
                        }

                        // 星级选择
                        RowLayout {
                            width: parent.width
                            spacing: Theme.spacingXSmall

                            Text {
                                text: qsTr("评分：")
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textSecondary
                                verticalAlignment: Text.AlignVCenter
                            }

                            StarRatingSelector {
                                rating: editRatingValue
                                starSize: Theme.fontSizeH1
                                onRatingModified: function(r) { editRatingValue = r }
                            }
                        }

                        // 评论输入
                        TextArea {
                            width: parent.width
                            height: 72
                            text: editComment
                            onTextChanged: editComment = text
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody
                            placeholderText: qsTr("修改你的评论...")
                            wrapMode: TextArea.WordWrap
                            background: Rectangle {
                                color: Theme.searchBarBackground
                                radius: Theme.radiusSmall
                                border.width: 1
                                border.color: Theme.dividerColor
                            }
                        }

                        // 操作按钮
                        RowLayout {
                            width: parent.width
                            spacing: Theme.spacingSmall

                            CustomButton {
                                Layout.fillWidth: true
                                buttonText: "\u2714 " + qsTr("保存修改")
                                buttonType: CustomButton.ButtonType.Primary
                                onClicked: {
                                    recipeVM.updateRating(recipeId, recipeVM.myRating.id, editRatingValue, editComment)
                                    editRatingId = 0
                                }
                            }

                            CustomButton {
                                Layout.fillWidth: true
                                buttonText: qsTr("取消")
                                buttonType: CustomButton.ButtonType.Secondary
                                onClicked: editRatingId = 0
                            }
                        }
                    }
                }

                // 操作按钮行（myRating 卡片下方）
                RowLayout {
                    width: parent.width
                    spacing: Theme.spacingSmall
                    visible: ratingsTriggered && recipeVM.myRating && recipeVM.myRating.id > 0 && editRatingId === 0

                    CustomButton {
                        Layout.fillWidth: true
                        buttonText: "\u270E " + qsTr("修改我的评分")
                        buttonType: CustomButton.ButtonType.Secondary
                        onClicked: {
                            editRatingId = recipeVM.myRating.id
                            editRatingValue = recipeVM.myRating.rating
                            editComment = recipeVM.myRating.comment
                        }
                    }

                    CustomButton {
                        Layout.fillWidth: true
                        buttonText: "\u2716 " + qsTr("删除我的评分")
                        buttonType: CustomButton.ButtonType.Secondary
                        onClicked: {
                            recipeVM.deleteRating(recipeId, recipeVM.myRating.id)
                        }
                    }
                }

                // 写评论表单（未评分时显示）
                Column {
                    width: parent.width
                    spacing: Theme.spacingSmall
                    visible: ratingsTriggered && !(recipeVM.myRating && recipeVM.myRating.id > 0)

                    // 星级选择
                    RowLayout {
                        width: parent.width

                        Text {
                            text: qsTr("我的评分：")
                            font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody
                            color: Theme.textPrimary
                        }

                        StarRatingSelector {
                            rating: newRating
                            starSize: Theme.fontSizeH2
                            onRatingModified: function(r) { newRating = r }
                        }
                    }

                    // 评论输入
                    TextArea {
                        width: parent.width
                        height: 80
                        placeholderText: qsTr("写下你的评论...")
                        text: newComment
                        onTextChanged: newComment = text
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        wrapMode: TextArea.WordWrap
                        background: Rectangle {
                            color: Theme.searchBarBackground
                            radius: Theme.radiusMedium
                        }
                    }

                    // 提交按钮
                    CustomButton {
                        width: parent.width
                        buttonText: "\u2605 " + qsTr("提交评分")
                        buttonType: CustomButton.ButtonType.Primary
                        enabled: newRating >= 1 && newRating <= 5
                        onClicked: {
                            recipeVM.rateRecipe(recipeId, newRating, newComment)
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.dividerColor
                    opacity: 0.5
                    visible: ratingsTriggered
                }

                // 评分卡片列表（过滤掉已置顶的本人评分）
                Repeater {
                    id: ratingsRepeater
                    model: {
                        var items = recipeVM.recipeRatings
                        if (!items || items.length === 0) return items
                        var myId = recipeVM.myRating ? recipeVM.myRating.id : 0
                        return myId > 0 ? items.filter(function(x) { return x.id !== myId }) : items
                    }

                    Rectangle {
                        width: parent ? parent.width : 200
                        height: ratingContent.implicitHeight + Theme.spacingSmall * 2
                        color: Theme.cardBackground
                        radius: Theme.radiusMedium

                        Column {
                            id: ratingContent
                            width: parent.width - Theme.spacingSmall * 2
                            anchors.centerIn: parent
                            spacing: Theme.spacingXSmall

                            // 第一行：用户名 + 星级 + 操作按钮
                            RowLayout {
                                width: parent.width

                                Text {
                                    text: modelData.username || qsTr("匿名用户")
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    font.weight: Theme.fontWeightMedium
                                    color: Theme.textPrimary
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: {
                                        var stars = ""
                                        var r = modelData.rating || 0
                                        for (var i = 1; i <= 5; i++)
                                            stars += (i <= r ? "\u2605" : "\u2606")
                                        return stars
                                    }
                                    font.pointSize: Theme.fontSizeBody
                                    color: Theme.warningColor
                                }

                                // 操作按钮
                                Row {
                                    spacing: 4

                                    ToolButton {
                                        width: 24
                                        height: 24
                                        flat: true
                                        text: "\u270E"
                                        font.pointSize: Theme.fontSizeSmall
                                        onClicked: {
                                            editRatingId = modelData.id
                                            editRatingValue = modelData.rating
                                            editComment = modelData.comment
                                        }
                                    }

                                    ToolButton {
                                        width: 24
                                        height: 24
                                        flat: true
                                        text: "\u2716"
                                        font.pointSize: Theme.fontSizeSmall
                                        onClicked: {
                                            recipeVM.deleteRating(recipeId, modelData.id)
                                        }
                                    }
                                }
                            }

                            // 编辑模式
                            Column {
                                width: parent.width
                                spacing: Theme.spacingXSmall
                                visible: editRatingId === modelData.id

                                StarRatingSelector {
                                    rating: editRatingValue
                                    starSize: Theme.fontSizeH2
                                    onRatingModified: function(r) { editRatingValue = r }
                                }

                                TextArea {
                                    width: parent.width
                                    height: 60
                                    text: editComment
                                    onTextChanged: editComment = text
                                    font.family: Theme.fontFamily
                                    font.pointSize: Theme.fontSizeBody
                                    background: Rectangle {
                                        color: Theme.searchBarBackground
                                        radius: Theme.radiusSmall
                                    }
                                }

                                RowLayout {
                                    width: parent.width
                                    CustomButton {
                                        buttonText: qsTr("保存")
                                        buttonType: CustomButton.ButtonType.Primary
                                        onClicked: {
                                            recipeVM.updateRating(recipeId, modelData.id, editRatingValue, editComment)
                                            editRatingId = 0
                                        }
                                    }
                                    CustomButton {
                                        buttonText: qsTr("取消")
                                        buttonType: CustomButton.ButtonType.Secondary
                                        onClicked: editRatingId = 0
                                    }
                                }
                            }

                            // 评论内容
                            Text {
                                width: parent.width
                                text: modelData.comment || ""
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textPrimary
                                wrapMode: Text.WordWrap
                                visible: editRatingId !== modelData.id && (modelData.comment || "").length > 0
                            }

                            // 时间戳
                            Text {
                                width: parent.width
                                text: modelData.createdAt || ""
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeSmall
                                color: Theme.textHint
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }

                // 加载更多
                CustomButton {
                    width: parent.width
                    buttonText: qsTr("加载更多评论")
                    buttonType: CustomButton.ButtonType.Secondary
                    visible: recipeVM.ratingsHasMore && !recipeVM.ratingsLoading
                    onClicked: recipeVM.loadMoreRatings()
                }

                // 空状态提示
                Text {
                    width: parent.width
                    text: qsTr("暂无评分和评论")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.fontSizeBody
                    color: Theme.textHint
                    horizontalAlignment: Text.AlignHCenter
                    visible: recipeVM.recipeRatings.length === 0 && !recipeVM.ratingsLoading && ratingsTriggered
                }
            }
        }
    }

    // 浮动导航栏（覆盖在内容上方）
    Rectangle {
        id: navBar
        width: parent.width
        height: 44
        z: 10
        color: "transparent"

        Rectangle {
            anchors.fill: parent
            color: Theme.cardBackground
            opacity: flickable.contentY > navThreshold ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.dividerColor
            opacity: flickable.contentY > navThreshold ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 0

            ToolButton {
                id: backBtn
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                flat: true
                contentItem: Canvas {
                    width: 22
                    height: 22
                    property color arrowColor: flickable.contentY > navThreshold ? Theme.textPrimary : "white"
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
                text: recipeVM.recipeDetail.name || ""
                font.pointSize: Theme.fontSizeBody
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
                elide: Text.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                opacity: flickable.contentY > navThreshold ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }

            ToolButton {
                id: moreBtn
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                flat: true
                text: "\u22EF"
                font.pointSize: 20
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: flickable.contentY > navThreshold ? Theme.textPrimary : "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: moreMenu.popup(moreBtn, 0, moreBtn.height)
            }
        }
    }

    // 底部操作栏
    Rectangle {
        id: bottomBar
        anchors.bottom: parent.bottom
        width: parent.width
        height: 56
        z: 10
        color: Theme.cardBackground
        // 加载中/失败时不显示操作栏（与内容区同步）
        visible: !recipeVM.detailLoading && !recipeVM.detailLoadFailed

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Theme.dividerColor
        }

        CustomButton {
            anchors.centerIn: parent
            width: parent.width - Theme.spacingMedium * 2
            buttonText: isFavorited ? "\u2605 " + qsTr("已收藏此菜谱") : "\u2606 " + qsTr("收藏此菜谱")
            buttonType: isFavorited ? CustomButton.ButtonType.Secondary : CustomButton.ButtonType.Primary
            onClicked: {
                if (isFavorited) {
                    // 已收藏 → 直接取消收藏
                    isFavorited = false
                    recipeVM.toggleFavorite(recipeId, 0)
                } else {
                    // 未收藏 → 弹出分组选择
                    recipeVM.loadFavoriteGroups()
                    groupDialog.open()
                }
            }
        }
    }

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
            text: (isFavorited ? "\u2605 " : "\u2606 ") + qsTr("收藏")
            font.pointSize: Theme.fontSizeBody
            contentItem: Label {
                text: parent.text
                font: parent.font
                color: Theme.textPrimary
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                moreMenu.close()
                if (isFavorited) {
                    isFavorited = false
                    recipeVM.toggleFavorite(recipeId, 0)
                } else {
                    recipeVM.loadFavoriteGroups()
                    groupDialog.open()
                }
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitWidth: 200
                implicitHeight: 1
                color: Theme.dividerColor
            }
        }

        MenuItem {
            text: qsTr("分享")
            font.pointSize: Theme.fontSizeBody
            contentItem: Label {
                text: parent.text
                font: parent.font
                color: Theme.textHint
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Connections {
        target: recipeVM
        function onRecipeDetailChanged() {
            var d = recipeVM.recipeDetail
            var ings = (d && d.ingredients) ? d.ingredients : []
            var stps = (d && d.steps) ? d.steps : []
            ingredientsRepeater.model = ings
            stepsRepeater.model = stps
            noIngredientsText.visible = (ings.length === 0)
            noStepsText.visible = (stps.length === 0)
            if (d && d.isFavorited !== undefined)
                isFavorited = d.isFavorited
        }
        function onRecipeVideosChanged() {
            videosRepeater.model = recipeVM.recipeVideos || []
        }
        function onRatingSubmitted() {
            newRating = 0
            newComment = ""
            recipeVM.loadRecipeRatings(recipeId, 1)
            recipeVM.loadMyRecipeRating(recipeId)
        }
        function onRatingUpdated(ratingId) {
            editRatingId = 0
            editRatingValue = 0
            editComment = ""
            recipeVM.loadRecipeRatings(recipeId, 1)
            recipeVM.loadMyRecipeRating(recipeId)
        }
        function onRatingDeleted(ratingId) {
            recipeVM.loadRecipeRatings(recipeId, 1)
            recipeVM.loadMyRecipeRating(recipeId)
        }
        function onFavoriteOperationFailed(error) {
            // 收藏写操作失败（toggleFavorite 已抑制全局提示，此处页内呈现，避免双弹）
            favoriteErrorBanner.text = ""
            favoriteErrorBanner.text = error
        }
    }

    // 收藏写操作失败提示（底部 toast；主内容失败由缓存/离线视图呈现，不重复提示）
    ErrorBanner {
        id: favoriteErrorBanner
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 100
        anchors.horizontalCenter: parent.horizontalCenter
    }

    GroupSelectionDialog {
        id: groupDialog
        groupsModel: recipeVM.favoriteGroups
        onGroupSelected: function(groupId) {
            isFavorited = true
            recipeVM.toggleFavorite(recipeId, groupId)
        }
    }
}
