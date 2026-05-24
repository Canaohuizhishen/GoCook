import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
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
    // 当前要显示的头像 URL（本地预览或服务端地址）
    property url    avatarDisplayUrl: ""

    // API 基础 URL（拼接头像等静态资源），来自 HttpGoCookApi 配置
    readonly property string apiBaseUrl: authViewModel.apiBaseUrl

    // ========== 调试信息（输出到终端） ==========
    function dbg(msg) { console.log("[QML-AVATAR] [" + new Date().toLocaleTimeString() + "] " + msg) }

    // ========== 原生文件选择器（调用系统对话框） ==========
    NativeFileDialog {
        id: avatarFileDialog
        onFileSelected: function(localPath) {
            profileEditPage.dbg("NativeFileDialog onFileSelected: " + localPath)

            pendingAvatarPath = localPath
            profileEditPage.dbg("pendingAvatarPath = " + pendingAvatarPath)
            statusText.text = qsTr("正在上传头像...")
            profileEditPage.avatarUploading = true
            profileEditPage.dbg("调用 uploadAvatar...")
            authViewModel.uploadAvatar(pendingAvatarPath)
            profileEditPage.dbg("uploadAvatar 调用完毕")
        }
        onRejected: {
            profileEditPage.dbg("NativeFileDialog onRejected (用户取消)")
        }
    }

    Component.onCompleted: {
        authViewModel.loadProfile()
    }

    Flickable {
        id: profileFlickable
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            id: column
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMedium
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingMedium
            spacing: Theme.spacingMedium

            // ========== 顶部间距 ==========
            Item { Layout.fillWidth: true; implicitHeight: Theme.spacingXLarge }

            // ========== 圆形头像 ==========
            Item {
                Layout.alignment: Qt.AlignHCenter
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
                    onClicked: avatarFileDialog.openWithFilter(
                        qsTr("选择头像图片"),
                        qsTr("图片文件 (*.jpg *.jpeg *.png *.gif *.bmp)"))
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
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("点击更换头像")
                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                color: Theme.textHint
            }

            // ★ 调试：显示当前头像 URL
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "URL: " + profileEditPage.avatarDisplayUrl
                font.family: Theme.fontFamily; font.pointSize: 9
                color: "gray"
                visible: profileEditPage.avatarDisplayUrl.toString().length > 0
                elide: Text.ElideMiddle
                maximumLineCount: 2
                wrapMode: Text.Wrap
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.dividerColor }

            // ========== 用户名 ==========
            ColumnLayout { Layout.fillWidth: true; spacing: Theme.spacingXSmall
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
            ColumnLayout { Layout.fillWidth: true; spacing: Theme.spacingXSmall
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
            ColumnLayout { Layout.fillWidth: true; spacing: Theme.spacingXSmall
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
            ColumnLayout { Layout.fillWidth: true; spacing: Theme.spacingXSmall
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

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.dividerColor }

            // ========== 状态提示 ==========
            Text {
                id: statusText; Layout.fillWidth: true; height: 20
                font.family: Theme.fontFamily; font.pointSize: Theme.fontSizeCaption
                color: Theme.accentColor; horizontalAlignment: Text.AlignHCenter
                visible: text.length > 0
            }

            // ========== 保存按钮 ==========
            CustomButton {
                Layout.fillWidth: true
                buttonText: qsTr("保存修改")
                buttonType: CustomButton.ButtonType.Primary
                onClicked: profileEditPage.doSaveProfile()
            }

            // ========== 返回按钮 ==========
            CustomButton {
                Layout.fillWidth: true
                buttonText: qsTr("返回")
                buttonType: CustomButton.ButtonType.Secondary
                onClicked: profileEditPage.goBack()
            }

            Item { width: 1; height: Theme.spacingXLarge }
        }
    }

    // 选择本地文件后，头像预览
    onSelectedFileUrlChanged: {
        if (selectedFileUrl.toString().length > 0)
            avatarDisplayUrl = selectedFileUrl
    }

    // 头像版本号变更时通过 onProfileChanged 处理（authViewModel 的属性通过 Connections 传递）


    Connections {
        target: authViewModel
        function onProfileChanged() {
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
            // 先清空再设回，强制 QML Image 重新加载（避免 httplib 不识别 ?t= 参数）
            profileEditPage.avatarDisplayUrl = ""
            Qt.callLater(function() {
                profileEditPage.avatarDisplayUrl = profileEditPage.apiBaseUrl + serverUrl
            })
            statusText.text = qsTr("头像已更新")
        }
        function onAvatarUploadFailed(error) {
            console.log("[QML-AVATAR] onAvatarUploadFailed: " + error)
            profileEditPage.avatarUploading = false
            statusText.text = qsTr("头像上传失败: ") + error
        }

    }
}
