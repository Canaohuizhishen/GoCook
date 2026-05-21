import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    id: announcementsPage
    title: qsTr("系统公告")

    signal goBack()

    Component.onCompleted: {
        announcementVM.loadAnnouncements(1)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingSmall

        // 返回按钮
        ToolButton {
            text: qsTr("← 返回")
            font.pointSize: Theme.fontSizeBody
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: Theme.primaryColor
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: announcementsPage.goBack()
        }

        // 标题
        Text {
            Layout.fillWidth: true
            text: qsTr("系统公告")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeH3
            font.bold: true
            color: Theme.textPrimary
        }

        // 空状态
        Text {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingLarge
            text: qsTr("暂无公告")
            font.family: Theme.fontFamily
            font.pointSize: Theme.fontSizeBody
            color: Theme.textHint
            horizontalAlignment: Text.AlignHCenter
            visible: !announcementVM.isLoading && announcementVM.announcements.length === 0
        }

        // 公告列表
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingSmall
            clip: true
            visible: announcementVM.announcements.length > 0

            model: announcementVM.announcements

            delegate: Rectangle {
                width: listView.width
                height: contentColumn.implicitHeight + Theme.spacingMedium * 2
                radius: Theme.radiusSmall
                color: Theme.cardBackground

                ColumnLayout {
                    id: contentColumn
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: Theme.spacingMedium
                    spacing: Theme.spacingXSmall

                    Text {
                        Layout.fillWidth: true
                        text: modelData.title || ""
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        font.bold: true
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.content || ""
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeCaption
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: modelData.createdAt || ""
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall
                        color: Theme.textHint
                    }
                }
            }

            footer: Item {
                width: listView.width
                height: 40
                visible: announcementVM.hasMore

                BusyIndicator {
                    anchors.centerIn: parent
                    running: announcementVM.isLoading
                    width: 20
                    height: 20
                }
            }

            onAtYEndChanged: {
                if (atYEnd && !announcementVM.isLoading && announcementVM.hasMore) {
                    announcementVM.loadNextPage()
                }
            }
        }
    }

    // 加载指示器
    LoadingIndicator {
        fullscreen: true
        message: qsTr("加载中...")
        isLoading: announcementVM.isLoading && announcementVM.announcements.length === 0
    }

    // 错误提示
    Connections {
        target: announcementVM
        function onErrorOccurred(error) {
            console.log("Announcement error:", error)
        }
    }
}
