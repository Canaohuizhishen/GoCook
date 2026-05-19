import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import client.styles
import "../components"

Page {
    id: profileEditPage
    title: qsTr("编辑个人资料")

    function goBack() {
        var item = profileEditPage.parent
        while (item) {
            try { if (typeof item.pop === "function") { item.pop(); return } } catch(e) {}
            item = item.parent
        }
        var sv = StackView.view
        if (sv && typeof sv.pop === "function") sv.pop()
    }

    function doSaveProfile() {
        statusText.text = qsTr("正在保存...")
        authViewModel.saveProfile(displayNameInput.text, emailInput.text, phoneInput.text)
    }

    property url    selectedFileUrl: ""
    property string pendingAvatarPath: ""
    property bool   avatarUploading: false

    // ========== 文件选择器 ==========
    FileDialog {
        id: avatarFileDialog
        title: qsTr("选择头像图片")
        nameFilters: [ qsTr("图片文件 (*.jpg *.jpeg *.png)") ]
        onAccepted: {
            var localPath = selectedFile.toLocalFile()
            if (localPath) {
                selectedFileUrl = selectedFile
                pendingAvatarPath = localPath
            } else {
                selectedFileUrl = selectedFile
                pendingAvatarPath = selectedFile.toString()
            }
        }
    }

    Component.onCompleted: authViewModel.loadProfile()

    Item {
        anchors.fill: parent
        ScrollView {
            anchors.fill: parent; clip: true; contentWidth: availableWidth
            Column {
                width: Math.min(parent.width - Theme.spacingLarge * 2, 400)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top; anchors.topMargin: Theme.spacingXLarge
                spacing: Theme.spacingMedium

                // ========== 圆形头像（Canvas 绘制，不依赖任何额外模块） ==========
                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 110; height: 110

                    // 背景圆
                    Rectangle {
                        anchors.fill: parent; radius: width / 2
                        color: Theme.cardBackground
                        border.color: Theme.dividerColor; border.width: 1
                    }

                    // 无头像时的占位文字
                    Text {
                        id: placeholderText
                        anchors.centerIn: parent
                        text: {
                            var n = authViewModel.profileDisplayName
                            if (n.length > 0) return n.charAt(0).toUpperCase()
                            n = authViewModel.username
                            if (n.length > 0) return n.charAt(0).toUpperCase()
                            return "?"
                        }
                        font.family: Theme.fontFamily; font.pointSize: 40
                        font.weight: Theme.fontWeightMedium; color: Theme.textHint
                    }

                    // Canvas 绘制圆形头像
                    Canvas {
                        id: avatarCanvas
                        anchors.fill: parent; anchors.margins: 3

                        property url pendingUrl: ""

                        onPendingUrlChanged: {
                            if (pendingUrl.toString().length > 0)
                                loadImage(pendingUrl)
                        }

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            var src = pendingUrl.toString()
                            if (src.length === 0 || !ctx.isImageReady(src)) {
                                placeholderText.visible = true
                                return
                            }

                            placeholderText.visible = false
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2, width / 2, 0, Math.PI * 2)
                            ctx.closePath()
                            ctx.clip()
                            ctx.drawImage(src, 0, 0, width, height)
                        }

                        onImageLoaded: requestPaint()
                    }

                    // selectedFileUrl 变化时通知 Canvas 加载
                    Connections {
                        target: profileEditPage
                        function onSelectedFileUrlChanged() {
                            avatarCanvas.pendingUrl = profileEditPage.selectedFileUrl
                        }
                    }
                    // authViewModel.profileAvatarUrl 变化时通知 Canvas 加载
                    Connections {
                        target: authViewModel
                        function onProfileChanged() {
                            var url = authViewModel.profileAvatarUrl
                            if (url.length > 0)
                                avatarCanvas.pendingUrl = "http://127.0.0.1:8080" + url
                        }
                    }

                    // 圆形边框
                    Rectangle {
                        anchors.fill: parent; radius: width / 2
                        color: "transparent"
                        border.color: Theme.dividerColor; border.width: 1.5
                    }

                    // 点击更换
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: avatarFileDialog.open()
                    }

                    // 编辑角标
                    Rectangle {
                        anchors.right: parent.right; anchors.bottom: parent.bottom
                        width: 32; height: 32; radius: 16; color: Theme.primaryColor; z: 1
                        Text {
                            anchors.centerIn: parent; text: "✎"
                            font.pointSize: 16; color: Theme.textOnPrimary
                        }
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("点击更换头像")
                    font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                    color: Theme.textHint
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.dividerColor }

                // ========== 用户名 ==========
                ColumnLayout { width: parent.width; spacing: Theme.spacingXSmall
                    Text { text: qsTr("用户名"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                    Rectangle {
                        Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                        color: Theme.searchBarBackground; border.color: Theme.dividerColor
                        TextInput {
                            anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                            verticalAlignment: TextInput.AlignVCenter
                            text: authViewModel.username; font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody; color: Theme.textHint
                            readOnly: true; selectByMouse: true
                        }
                    }
                }

                // ========== 昵称 ==========
                ColumnLayout { width: parent.width; spacing: Theme.spacingXSmall
                    Text { text: qsTr("昵称"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                    Rectangle {
                        Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                        color: Theme.cardBackground; border.color: Theme.dividerColor
                        TextInput {
                            id: displayNameInput
                            anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                            verticalAlignment: TextInput.AlignVCenter
                            text: authViewModel.profileDisplayName; font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                            clip: true; selectByMouse: true
                        }
                    }
                }

                // ========== 邮箱 ==========
                ColumnLayout { width: parent.width; spacing: Theme.spacingXSmall
                    Text { text: qsTr("邮箱"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                    Rectangle {
                        Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                        color: Theme.cardBackground; border.color: Theme.dividerColor
                        TextInput {
                            id: emailInput
                            anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                            verticalAlignment: TextInput.AlignVCenter
                            text: authViewModel.profileEmail; font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                            clip: true; selectByMouse: true; inputMethodHints: Qt.ImhEmailCharactersOnly
                        }
                    }
                }

                // ========== 手机号 ==========
                ColumnLayout { width: parent.width; spacing: Theme.spacingXSmall
                    Text { text: qsTr("手机号"); font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption; color: Theme.textHint }
                    Rectangle {
                        Layout.fillWidth: true; height: 50; radius: Theme.radiusMedium
                        color: Theme.cardBackground; border.color: Theme.dividerColor
                        TextInput {
                            id: phoneInput
                            anchors.fill: parent; anchors.leftMargin: Theme.spacingMedium; anchors.rightMargin: Theme.spacingMedium
                            verticalAlignment: TextInput.AlignVCenter
                            text: authViewModel.profilePhone; font.family: Theme.fontFamily
                            font.pointSize: Theme.fontSizeBody; color: Theme.textPrimary
                            clip: true; selectByMouse: true; inputMethodHints: Qt.ImhDialableCharactersOnly
                        }
                    }
                }

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
                    text: qsTr("保存修改")
                    background: Rectangle { radius: Theme.radiusMedium; color: Theme.primaryColor }
                    contentItem: Text {
                        text: saveBtn.text; font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium
                        color: Theme.textOnPrimary; horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (profileEditPage.pendingAvatarPath.length > 0) {
                            statusText.text = qsTr("正在上传头像...")
                            profileEditPage.avatarUploading = true
                            authViewModel.uploadAvatar(profileEditPage.pendingAvatarPath)
                        } else {
                            profileEditPage.doSaveProfile()
                        }
                    }
                }

                // ========== 返回按钮 ==========
                Button {
                    id: backBtn; width: parent.width; height: 50; text: qsTr("返回")
                    background: Rectangle {
                        radius: Theme.radiusMedium; color: "transparent"
                        border.color: Theme.primaryColor; border.width: 1
                    }
                    contentItem: Text {
                        text: backBtn.text; font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody; color: Theme.primaryColor
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: profileEditPage.goBack()
                }

                Item { width: 1; height: Theme.spacingXLarge }
            }
        }
    }

    Connections {
        target: authViewModel
        function onProfileSaved() {
            statusText.text = qsTr("保存完成")
            profileEditPage.goBack()
        }
        function onProfileSaveFailed(error) {
            statusText.text = qsTr("保存失败: ") + error
        }
        function onAvatarUploaded(serverUrl) {
            profileEditPage.selectedFileUrl = ""
            profileEditPage.pendingAvatarPath = ""
            profileEditPage.avatarUploading = false
            profileEditPage.doSaveProfile()
        }
        function onAvatarUploadFailed(error) {
            profileEditPage.avatarUploading = false
            statusText.text = qsTr("头像上传失败: ") + error
        }
    }
}
