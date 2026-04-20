import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: qsTr("个人中心")

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20

        Label {
            text: authViewModel.loggedIn ? "当前用户：" + authViewModel.username : "未登录"
            font.pixelSize: 16
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            text: qsTr("退出登录")
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                authViewModel.logout()
            }
        }
    }
}