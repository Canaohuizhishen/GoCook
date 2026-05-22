import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "../components"

Page {
    id: settingsPage
    title: qsTr("设置")

    signal editProfileRequest()
    signal accountSecurityRequest()
    signal dietaryPreferencesRequest()
    signal healthProfileRequest()

    function goBack() {
        var sv = StackView.view
        if (sv && typeof sv.pop === "function") sv.pop()
    }

    // ========== 顶部导航栏（返回按钮 + 标题） ==========
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 48
        color: "transparent"

        Label {
            anchors.centerIn: parent
            text: qsTr("设置")
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
            onClicked: goBack()
        }
    }

    // ========== 居中容器 ==========
    Item {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        anchors.topMargin: 48 + Theme.spacingLarge

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: Theme.spacingSmall

            // ========== 编辑个人资料 ==========
            Rectangle {
                Layout.fillWidth: true
                height: 52
                radius: Theme.radiusMedium
                color: Theme.cardBackground
                border.color: Theme.dividerColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMedium
                    anchors.rightMargin: Theme.spacingMedium
                    spacing: Theme.spacingMedium

                    Canvas {
                        width: 22
                        height: 22
                        property color iconColor: Theme.textPrimary
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1.8
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.arc(11, 6, 4, 0, Math.PI * 2)
                            ctx.moveTo(11, 10); ctx.lineTo(11, 16)
                            ctx.moveTo(5, 12); ctx.lineTo(17, 12)
                            ctx.moveTo(11, 16); ctx.lineTo(7, 21)
                            ctx.moveTo(11, 16); ctx.lineTo(15, 21)
                            ctx.stroke()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("编辑个人资料")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        text: "›"
                        font.pointSize: 24
                        color: Theme.textHint
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsPage.editProfileRequest()
                }
            }

            // ========== 账号与安全 ==========
            Rectangle {
                Layout.fillWidth: true
                height: 52
                radius: Theme.radiusMedium
                color: Theme.cardBackground
                border.color: Theme.dividerColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMedium
                    anchors.rightMargin: Theme.spacingMedium
                    spacing: Theme.spacingMedium

                    Canvas {
                        width: 22
                        height: 22
                        property color iconColor: Theme.textPrimary
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1.8
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.moveTo(8, 10); ctx.lineTo(8, 7)
                            ctx.arc(11, 7, 3, Math.PI, 0, false)
                            ctx.lineTo(14, 10)
                            ctx.moveTo(6, 10); ctx.lineTo(6, 17)
                            ctx.lineTo(16, 17); ctx.lineTo(16, 10)
                            ctx.closePath()
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.arc(11, 13, 1.5, 0, Math.PI * 2)
                            ctx.moveTo(11, 14.5); ctx.lineTo(11, 16)
                            ctx.stroke()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("账号与安全")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        text: "›"
                        font.pointSize: 24
                        color: Theme.textHint
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsPage.accountSecurityRequest()
                }
            }

            // ========== 饮食偏好设置 ==========
            Rectangle {
                Layout.fillWidth: true
                height: 52
                radius: Theme.radiusMedium
                color: Theme.cardBackground
                border.color: Theme.dividerColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMedium
                    anchors.rightMargin: Theme.spacingMedium
                    spacing: Theme.spacingMedium

                    Canvas {
                        width: 22
                        height: 22
                        property color iconColor: Theme.textPrimary
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1.8
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.moveTo(11, 9); ctx.lineTo(11, 19)
                            ctx.moveTo(7, 4); ctx.lineTo(7, 10)
                            ctx.moveTo(15, 4); ctx.lineTo(15, 10)
                            ctx.moveTo(11, 4); ctx.lineTo(11, 7)
                            ctx.moveTo(7, 4); ctx.lineTo(15, 4)
                            ctx.stroke()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("饮食偏好设置")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        text: "›"
                        font.pointSize: 24
                        color: Theme.textHint
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsPage.dietaryPreferencesRequest()
                }
            }

            // ========== 健康指标 ==========
            Rectangle {
                Layout.fillWidth: true
                height: 52
                radius: Theme.radiusMedium
                color: Theme.cardBackground
                border.color: Theme.dividerColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMedium
                    anchors.rightMargin: Theme.spacingMedium
                    spacing: Theme.spacingMedium

                    Canvas {
                        width: 22
                        height: 22
                        property color iconColor: Theme.textPrimary
                        onIconColorChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1.8
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.moveTo(11, 18)
                            ctx.bezierCurveTo(3, 11, 3, 5, 6.5, 3)
                            ctx.bezierCurveTo(9, 1.5, 11, 4, 11, 4)
                            ctx.bezierCurveTo(11, 4, 13, 1.5, 15.5, 3)
                            ctx.bezierCurveTo(19, 5, 19, 11, 11, 18)
                            ctx.stroke()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("健康指标")
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        text: "›"
                        font.pointSize: 24
                        color: Theme.textHint
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsPage.healthProfileRequest()
                }
            }

            // ========== 主题模式（可展开） ==========
            Rectangle {
                id: themeItem
                Layout.fillWidth: true
                radius: Theme.radiusMedium
                color: Theme.cardBackground
                border.color: Theme.dividerColor
                border.width: 1
                clip: true

                // 显式高度计算，避免布局引擎循环依赖
                readonly property int headerHeight: 52
                readonly property int optionRowHeight: 40
                readonly property int dividerHeight: 1
                readonly property int expandedPadding: 8  // top + bottom
                readonly property int expandedContentHeight: dividerHeight + optionRowHeight * 3 + expandedPadding

                implicitHeight: headerHeight + (themeExpanded.visible ? expandedContentHeight : 0)

                Column {
                    anchors.fill: parent
                    spacing: 0
                    topPadding: 0; bottomPadding: 0; leftPadding: 0; rightPadding: 0

                    // 点击头部
                    Rectangle {
                        width: parent.width
                        height: themeItem.headerHeight
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMedium
                            anchors.rightMargin: Theme.spacingMedium
                            spacing: Theme.spacingMedium

                            Canvas {
                                width: 22
                                height: 22
                                property color iconColor: Theme.textPrimary
                                onIconColorChanged: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.strokeStyle = iconColor
                                    ctx.lineWidth = 1.8
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"
                                    ctx.beginPath()
                                    ctx.arc(11, 11, 4, 0, Math.PI * 2)
                                    for (var i = 0; i < 8; i++) {
                                        var a = i * Math.PI / 4
                                        ctx.moveTo(11 + 6 * Math.cos(a), 11 + 6 * Math.sin(a))
                                        ctx.lineTo(11 + 8 * Math.cos(a), 11 + 8 * Math.sin(a))
                                    }
                                    ctx.stroke()
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("主题模式")
                                font.family: Theme.fontFamily
                                font.pointSize: Theme.fontSizeBody
                                color: Theme.textPrimary
                                verticalAlignment: Text.AlignVCenter
                            }

                            Text {
                                text: themeExpanded.visible ? "▾" : "▸"
                                font.pointSize: 18
                                color: Theme.textHint
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: themeExpanded.visible = !themeExpanded.visible
                        }
                    }

                    // 展开选项
                    Column {
                        id: themeExpanded
                        width: parent.width
                        visible: false
                        spacing: 0
                        topPadding: 0; bottomPadding: 0; leftPadding: 0; rightPadding: 0

                        Rectangle {
                            width: parent.width
                            height: themeItem.dividerHeight
                            color: Theme.dividerColor
                        }

                        Column {
                            width: parent.width
                            spacing: 0
                            leftPadding: Theme.spacingLarge
                            rightPadding: Theme.spacingMedium
                            topPadding: 4
                            bottomPadding: 4

                            Repeater {
                                model: [
                                    { label: qsTr("跟随系统"), modeVal: Theme.themeModeSystem },
                                    { label: qsTr("浅色"),     modeVal: Theme.themeModeLight },
                                    { label: qsTr("深色"),     modeVal: Theme.themeModeDark }
                                ]

                                delegate: Rectangle {
                                    required property var modelData
                                    readonly property bool isSelected: Theme.themeMode === modelData.modeVal

                                    width: parent.width - parent.leftPadding - parent.rightPadding
                                    height: themeItem.optionRowHeight
                                    radius: 6
                                    color: mouseArea.containsMouse ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.08) : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 10

                                        // 自定义单选圆圈
                                        Rectangle {
                                            width: 18
                                            height: 18
                                            radius: 9
                                            border.width: 2
                                            border.color: isSelected ? Theme.primaryColor : Theme.textHint
                                            color: "transparent"

                                            Rectangle {
                                                anchors.centerIn: parent
                                                width: 10
                                                height: 10
                                                radius: 5
                                                color: Theme.primaryColor
                                                visible: isSelected
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            font.family: Theme.fontFamily
                                            font.pointSize: Theme.fontSizeBody
                                            color: Theme.textPrimary
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    MouseArea {
                                        id: mouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Theme.themeMode = modelData.modeVal
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
