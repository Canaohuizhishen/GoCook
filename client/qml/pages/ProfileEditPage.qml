import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import client
import "../components"

Page {
    id: profileEditPage
    title: qsTr("编辑个人资料")

    signal showPreferencesRequest()
    signal showHealthProfileRequest()

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
    // 当前要显示的头像 URL（本地预览或服务端地址）
    property url    avatarDisplayUrl: ""

    // API 基础 URL（拼接头像等静态资源），来自 HttpGoCookApi 配置
    readonly property string apiBaseUrl: authViewModel.apiBaseUrl

    // ========== 调试信息（输出到终端） ==========
    function dbg(msg) { console.log("[QML-AVATAR] [" + new Date().toLocaleTimeString() + "] " + msg) }

    // ========== 文件选择器 ==========
    FileDialog {
        id: avatarFileDialog
        title: qsTr("选择头像图片")
        nameFilters: [ qsTr("图片文件 (*.jpg *.jpeg *.png *.gif *.bmp *.webp)") ]
        onAccepted: {
            profileEditPage.dbg("FileDialog onAccepted 触发")

            // 兼容 Qt 6.0+ 不同版本：
            // - Qt 6.0-6.2: selectedFile (单数)
            // - Qt 6.3+:    selectedFile 已弃用，改用 selectedFiles (数组)
            var chosenUrl = selectedFile;
            if (typeof selectedFiles !== "undefined" && selectedFiles.length > 0) {
                chosenUrl = selectedFiles[0];
            }
            profileEditPage.dbg("chosenUrl: " + chosenUrl)

            var localPath = chosenUrl.toLocalFile()
            profileEditPage.dbg("toLocalFile: " + (localPath || "(空)"))
            if (localPath) {
                selectedFileUrl = chosenUrl
                pendingAvatarPath = localPath
            } else {
                selectedFileUrl = chosenUrl
                pendingAvatarPath = chosenUrl.toString()
                profileEditPage.dbg("localPath 为空，使用 toString()")
            }
            profileEditPage.dbg("pendingAvatarPath = " + pendingAvatarPath)
            statusText.text = qsTr("正在上传头像...")
            profileEditPage.avatarUploading = true
            profileEditPage.dbg("调用 uploadAvatar...")
            authViewModel.uploadAvatar(profileEditPage.pendingAvatarPath)
            profileEditPage.dbg("uploadAvatar 调用完毕")
        }
        onRejected: {
            profileEditPage.dbg("FileDialog onRejected (用户取消)")
        }
    }

    Component.onCompleted: {
        authViewModel.loadProfile()
    }

    Item {
        anchors.fill: parent
        ScrollView {
            anchors.fill: parent; clip: true; contentWidth: availableWidth
            Column {
                width: Math.min(parent.width - Theme.spacingLarge * 2, 400)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top; anchors.topMargin: Theme.spacingXLarge
                spacing: Theme.spacingMedium

                // ========== 圆形头像 ==========
                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 110; height: 110

                    CircularImage {
                        id: avatarImage
                        anchors.fill: parent
                        borderColor: Theme.dividerColor
                        borderWidth: 1.5
                        source: profileEditPage.avatarDisplayUrl
                        placeholderFallback: {
                            var n = authViewModel.profileDisplayName
                            if (n.length > 0) return n.charAt(0).toUpperCase()
                            n = authViewModel.username
                            if (n.length > 0) return n.charAt(0).toUpperCase()
                            return "?"
                        }
                        placeholderText.font.family: Theme.fontFamily
                        placeholderText.font.pointSize: 40
                        placeholderText.font.weight: Theme.fontWeightMedium
                        placeholderText.color: Theme.textHint
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

                Rectangle { width: parent.width; height: 1; color: Theme.dividerColor }

                // ========== 状态提示 ==========
                Text {
                    id: statusText; width: parent.width; height: 20
                    font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                    color: Theme.accentColor; horizontalAlignment: Text.AlignHCenter
                    visible: text.length > 0
                }

                // ========== 饮食偏好设置入口 ==========
                Button {
                    id: prefBtn; width: parent.width; height: 50
                    text: qsTr("饮食偏好设置")
                    background: Rectangle { radius: Theme.radiusMedium; color: Theme.searchBarBackground; border.color: Theme.dividerColor; border.width: 1 }
                    contentItem: Text {
                        text: prefBtn.text; font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium
                        color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        profileEditPage.showPreferencesRequest()
                    }
                }

                // ========== 健康指标设置入口 ==========
                Button {
                    id: healthBtn; width: parent.width; height: 50
                    text: qsTr("健康指标")
                    background: Rectangle { radius: Theme.radiusMedium; color: Theme.searchBarBackground; border.color: Theme.dividerColor; border.width: 1 }
                    contentItem: Text {
                        text: healthBtn.text; font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody; font.weight: Theme.fontWeightMedium
                        color: Theme.textPrimary; horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        profileEditPage.showHealthProfileRequest()
                    }
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
                        profileEditPage.doSaveProfile()
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

    // 选择本地文件后，头像预览
    onSelectedFileUrlChanged: {
        if (selectedFileUrl.toString().length > 0)
            avatarDisplayUrl = selectedFileUrl
    }

    Connections {
        target: authViewModel
        function onProfileChanged() {
            console.log("[QML-AVATAR] onProfileChanged fired, profileAvatarUrl='" + authViewModel.profileAvatarUrl + "'")
            var url = authViewModel.profileAvatarUrl
            if (url.length > 0)
                profileEditPage.avatarDisplayUrl = profileEditPage.apiBaseUrl + url
        }
        function onProfileSaved() {
            statusText.text = qsTr("保存完成")
            profileEditPage.goBack()
        }
        function onProfileSaveFailed(error) {
            console.log("[QML-AVATAR] onProfileSaveFailed: " + error)
            statusText.text = qsTr("保存失败: ") + error
        }
        function onAvatarUploaded(serverUrl) {
            console.log("[QML-AVATAR] onAvatarUploaded: serverUrl='" + serverUrl + "'")
            profileEditPage.selectedFileUrl = ""
            profileEditPage.pendingAvatarPath = ""
            profileEditPage.avatarUploading = false
            profileEditPage.avatarDisplayUrl = profileEditPage.apiBaseUrl + serverUrl
            statusText.text = qsTr("头像已更新")
        }
        function onAvatarUploadFailed(error) {
            console.log("[QML-AVATAR] onAvatarUploadFailed: " + error)
            profileEditPage.avatarUploading = false
            statusText.text = qsTr("头像上传失败: ") + error
        }

    }
}
