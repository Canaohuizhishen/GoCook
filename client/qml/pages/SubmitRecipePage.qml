import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: page
    title: recipeId > 0 ? qsTr("编辑菜谱") : qsTr("提交菜谱")

    property int recipeId: 0
    property var _stackView
    property bool submitting: false
    property bool uploadingImages: false
    property int uploadTotal: 0
    property int uploadDone: 0
    property string coverImagePath: ""  // 新建菜谱时预选的封面图本地路径
    // 步骤图统一由 stepListModel 的字段管理：
    //   imageDisplayUrl → 展示用完整 URL（file:/// 或 http://）
    //   localImagePath  → 本地路径（上传时用）
    property string uploadedCoverUrl: ""  // 展示用（含 baseUrl 前缀）
    property string _relCoverUrl: ""      // API 用（相对路径）
    property string recipeStatus: ""      // 编辑模式：pending / approved
    property int uploadQueueIndex: -1   // -1=空闲, -2=上传封面, >=0=上传步骤图
    property int _submittedRecipeId: 0  // 新建菜谱返回的 id，供后续上传使用

    Component.onCompleted: {
        if (recipeId > 0) {
            recipeVM.loadRecipeForEdit(recipeId)
        }
    }

    function prePopulateForm() {
        var detail = recipeVM.recipeDetail
        if (!detail || Object.keys(detail).length === 0) return

        nameField.text = detail.name || ""
        descField.text = detail.description || ""
        if (detail.imageUrl) {
            _relCoverUrl = detail.imageUrl
            coverPreview.source = authViewModel.apiBaseUrl + detail.imageUrl
            uploadedCoverUrl = authViewModel.apiBaseUrl + detail.imageUrl
        } else {
            _relCoverUrl = ""
            coverPreview.source = ""
            uploadedCoverUrl = ""
        }

        // 预填充食材
        ingredientListModel.clear()
        var ingredients = detail.ingredients
        if (ingredients && ingredients.length) {
            for (var i = 0; i < ingredients.length; i++) {
                ingredientListModel.append(ingredients[i])
            }
        }

        // 预填充步骤
        stepListModel.clear()
        var steps = detail.steps
        if (steps && steps.length) {
            for (var j = 0; j < steps.length; j++) {
                var step = steps[j]
                step.imageDisplayUrl = step.image_url ? authViewModel.apiBaseUrl + step.image_url : ""
                step.localImagePath = ""  // 确保步骤有 localImagePath 属性，与发布模式一致
                stepListModel.append(step)
            }
        }
    }

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
                text: recipeId > 0 ? qsTr("编辑菜谱") : qsTr("提交菜谱")
                font.pointSize: Theme.fontSizeH1
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

            // 封面图片预览 + 更换按钮
            Rectangle {
                width: parent.width
                height: 160
                radius: Theme.radiusMedium
                color: Theme.cardBackground
                border.color: Theme.dividerColor
                border.width: 1

                Image {
                    id: coverPreview
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    source: coverImagePath ? "file:///" + coverImagePath : ""
                }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("点击选择封面图片")
                    color: Theme.textHint
                    font.pointSize: Theme.fontSizeBody
                    visible: coverPreview.source === ""
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: coverImagePicker.open()
                }

                NativeFileDialog {
                    id: coverImagePicker
                    onFileSelected: function(path) {
                        coverImagePath = path
                        coverPreview.source = "file:///" + path
                    }
                }
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
                        spacing: Theme.spacingSmall

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("步骤%1: %2").arg(order).arg(description)
                            font.pointSize: Theme.fontSizeBody
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }

                        // 步骤图缩略图
                        Image {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            visible: model.imageDisplayUrl ? model.imageDisplayUrl !== "" : false
                            source: model.imageDisplayUrl || ""
                        }

                        Button {
                            text: model.imageDisplayUrl ? qsTr("换图") : qsTr("步骤图")
                            flat: true
                            font.pointSize: Theme.fontSizeSmall
                            onClicked: {
                                stepImagePicker.pendingStepIndex = index
                                stepImagePicker.open()
                            }
                        }

                        Button {
                            text: qsTr("删除")
                            flat: true
                            font.pointSize: Theme.fontSizeSmall
                            onClicked: {
                                stepListModel.remove(index)
                            }
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
                buttonText: uploadingImages ? qsTr("正在上传图片 %1/%2...").arg(uploadDone).arg(uploadTotal)
                            : submitting ? qsTr("保存中...")
                            : (recipeId > 0 ? qsTr("保存修改") : qsTr("提交菜谱"))
                enabled: nameField.text.trim() !== "" && !submitting && !uploadingImages
                        && ingredientListModel.count > 0 && stepListModel.count > 0
                onClicked: {
                    submitting = true
                    doSaveMetadata()
                }
            }

            // 删除按钮（仅待审核菜谱可见）
            CustomButton {
                width: parent.width
                buttonText: qsTr("删除菜谱")
                buttonColor: Theme.errorColor
                visible: recipeId > 0 && recipeStatus === "pending"
                enabled: !submitting
                onClicked: confirmDeleteDialog.open()
            }

            Item { height: Theme.spacingLarge }
        }
    }

    // 删除确认弹窗
    Dialog {
        id: confirmDeleteDialog
        title: qsTr("确认删除")
        anchors.centerIn: parent
        modal: true
        width: Math.min(parent.width * 0.85, 340)
        standardButtons: Dialog.NoButton

        background: Rectangle {
            color: Theme.cardBackground
            radius: Theme.radiusMedium
            border.color: Theme.dividerColor
            border.width: 1
        }

        ColumnLayout {
            spacing: Theme.spacingMedium
            width: parent.width

            Text {
                text: qsTr("确定要删除这个菜谱吗？此操作不可撤销。")
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: Theme.spacingSmall
                Layout.fillWidth: true

                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("取消")
                    buttonType: CustomButton.ButtonType.Secondary
                    onClicked: confirmDeleteDialog.close()
                }
                CustomButton {
                    Layout.fillWidth: true
                    buttonText: qsTr("确认删除")
                    buttonColor: Theme.errorColor
                    onClicked: {
                        confirmDeleteDialog.close()
                        recipeVM.deleteRecipe(recipeId)
                    }
                }
            }
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
                        description: stepDescField.text.trim(),
                        image_url: "",
                        imageDisplayUrl: "",
                        localImagePath: ""
                    })
                    stepDescField.clear()
                    stepDialog.close()
                }
            }
        }
    }

    // 步骤图选择器（复用，通过 pendingStepIndex 指定目标步骤）
    NativeFileDialog {
        id: stepImagePicker
        property int pendingStepIndex: -1
        onFileSelected: function(path) {
            if (pendingStepIndex >= 0) {
                var entry = stepListModel.get(pendingStepIndex)
                if (!entry) entry = {}
                entry.imageDisplayUrl = "file:///" + path   // 立即显示本地预览
                entry.localImagePath = path                  // 标记待上传，保存时统一上传
                stepListModel.set(pendingStepIndex, entry)
                pendingStepIndex = -1
            }
        }
        onRejected: { pendingStepIndex = -1 }
    }

    // 执行保存元数据
    function doSaveMetadata() {
        submitting = true
        var ingredients = []
        var steps = []
        for (var i = 0; i < ingredientListModel.count; i++) {
            var ing = ingredientListModel.get(i)
            ingredients.push({name: ing.name, quantity: ing.quantity, unit: ing.unit})
        }
        for (var j = 0; j < stepListModel.count; j++) {
            var stepData = stepListModel.get(j)
            var stepObj = {
                order: stepData.order,
                description: stepData.description,
                image_url: (stepData.image_url && stepData.image_url !== "") ? stepData.image_url : ""
            }
            steps.push(stepObj)
        }

        if (recipeId > 0) {
            // 编辑模式：图片已上传完成，此时 model 中有正确 image_url
            recipeVM.editRecipe(
                recipeId,
                nameField.text.trim(),
                descField.text.trim(),
                _relCoverUrl,
                ingredients,
                steps,
                []
            )
        } else {
            // 新建模式：先创建菜谱（无图），再逐个上传图片
            recipeVM.submitRecipe(
                nameField.text.trim(),
                descField.text.trim(),
                "",
                ingredients,
                steps,
                []
            )
        }
    }

    Connections {
        target: recipeVM
        function onRecipeSubmitted(id, status) {
            _submittedRecipeId = id
            // 统计待上传图片数
            var toUpload = 0
            for (var i = 0; i < stepListModel.count; i++) {
                var e = stepListModel.get(i)
                if (e && e.localImagePath) toUpload++
            }
            uploadTotal = coverImagePath !== "" ? toUpload + 1 : toUpload
            uploadDone = 0
            uploadingImages = (uploadTotal > 0)

            if (coverImagePath !== "") {
                recipeVM.uploadRecipeImage(id, coverImagePath)
            } else if (uploadTotal > 0) {
                page.uploadQueueIndex = -2
                page.startNextStepUpload()
            } else {
                submitting = false
                snackBar.show(qsTr("提交成功，等待审核"), "success")
                popTimer.start()
            }
        }
        function onSubmitFailed(error) {
            submitting = false
            snackBar.show(qsTr("提交失败，请稍后重试"), "error")
        }
        function onRecipeDeleted() {
            submitting = false
            snackBar.show(qsTr("菜谱已删除"), "success")
            popTimer.interval = 400
            popTimer.start()
        }
        function onDeleteFailed(error) {
            submitting = false
            snackBar.show(qsTr("删除失败: ") + error, "error")
        }
        function onRecipeImageUploaded(imageUrl) {
            _relCoverUrl = imageUrl
            uploadedCoverUrl = authViewModel.apiBaseUrl + imageUrl
            coverPreview.source = uploadedCoverUrl
            uploadDone++
            // 上传步骤图
            page.uploadQueueIndex = -2  // 封面完成
            page.startNextStepUpload()
        }
        function onRecipeImageUploadFailed(error) {
            uploadingImages = false
            submitting = false
            snackBar.show(qsTr("封面上传失败: ") + error, "error")
        }
        function onStepImageUploaded(stepIndex, imageUrl) {
            // 将服务端返回的步骤图 URL 回填到模型
            if (imageUrl && imageUrl !== "") {
                var item = stepListModel.get(stepIndex)
                if (item) {
                    item.imageDisplayUrl = authViewModel.apiBaseUrl + imageUrl
                    item.image_url = imageUrl
                    item.localImagePath = ""  // 上传完成，清本地路径
                    stepListModel.set(stepIndex, item)
                }
            }
            // 推进上传队列
            if (uploadingImages) {
                uploadDone++
                page.startNextStepUpload()
            }
        }
        function onStepImageUploadFailed(stepIndex, error) {
            uploadingImages = false
            submitting = false
            snackBar.show(qsTr("步骤") + (stepIndex+1) + qsTr("图片上传失败: ") + error, "error")
        }
        function onRecipeEdited() {
            // 编辑模式：先保存元数据，再上传步骤图（与新建模式逻辑一致）
            var toUpload = 0
            for (var i = 0; i < stepListModel.count; i++) {
                var e = stepListModel.get(i)
                if (e && e.localImagePath) toUpload++
            }
            uploadTotal = coverImagePath !== "" ? toUpload + 1 : toUpload
            uploadDone = 0
            uploadingImages = (uploadTotal > 0)

            if (coverImagePath !== "") {
                recipeVM.uploadRecipeImage(recipeId, coverImagePath)
            } else if (uploadTotal > 0) {
                uploadQueueIndex = -2
                startNextStepUpload()
            } else {
                submitting = false
                snackBar.show(qsTr("更新成功，等待重新审核"), "success")
                popTimer.start()
            }
        }
        function onEditFailed(error) {
            submitting = false
            snackBar.show(qsTr("修改失败，请稍后重试"), "error")
        }
        function onEditFormDataReady() {
            page.prePopulateForm()
        }
    }

    // 顺序上传步骤图
    function startNextStepUpload() {
        uploadQueueIndex++
        while (uploadQueueIndex < stepListModel.count) {
            var entry = stepListModel.get(uploadQueueIndex)
            if (entry && entry.localImagePath) {
                var rid = recipeId > 0 ? recipeId : (page._submittedRecipeId || 0)
                if (rid === 0) {
                    rid = (recipeVM.recipeDetail && recipeVM.recipeDetail.id || 0)
                }
                if (rid > 0) {
                    recipeVM.uploadStepImage(rid, uploadQueueIndex, entry.localImagePath)
                    return
                }
            }
            uploadQueueIndex++
        }
        // 所有图片上传完毕
        submitting = false
        uploadingImages = false
        snackBar.show(qsTr("保存成功"), "success")
        popTimer.start()
    }

    Timer {
        id: popTimer
        interval: 400
        onTriggered: {
            snackBar.hide()
            _stackView.pop()
        }
    }

    Item {
        id: snackBar
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.382 - height / 2
        width: shadow.width
        height: shadow.height
        opacity: 0
        visible: opacity > 0

        Rectangle {
            id: shadow
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 4
            width: card.width
            height: card.height
            radius: Theme.radiusLarge
            color: Theme.cardShadowColor
        }

        Rectangle {
            id: card
            anchors.centerIn: parent
            width: Math.min(Math.max(msgText.implicitWidth + Theme.spacingLarge * 3, 200), 300)
            height: msgText.implicitHeight + Theme.spacingLarge * 2 + Theme.spacingMedium
            radius: Theme.radiusLarge
            color: Theme.cardBackground
            border.width: 2
            border.color: Theme.accentColor
            Behavior on border.color { ColorAnimation { duration: Theme.durationShort } }
        }

        Text {
            id: msgText
            anchors.centerIn: card
            width: card.width - Theme.spacingLarge * 2
            color: Theme.textPrimary
            font.pointSize: Theme.fontSizeH3
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }

        function show(msg, type) {
            msgText.text = msg
            if (type === "error") {
                card.border.color = Theme.errorColor
            } else {
                card.border.color = Theme.accentColor
            }
            opacity = 1
        }
        function hide() {
            opacity = 0
        }

        Behavior on opacity { NumberAnimation { duration: Theme.durationMedium } }
    }
}
