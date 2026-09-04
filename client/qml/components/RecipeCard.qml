import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import client

Rectangle {
    id: card

    // 外部可绑定的属性
    property alias recipeName: nameLabel.text
    property alias recipeDescription: descLabel.text
    property alias imageSource: recipeImage.source
    property alias prepTime: prepTimeLabel.text
    property alias cookTime: cookTimeLabel.text
    property alias tags: tagsRepeater.model

    // 推荐数据（可选，默认隐藏）
    property bool showMatch: false
    property real matchScore: 0.0
    property string healthNotice: ""  // 健康软提示全文（仅推荐模式传入）：卡片上收敛为小徽标，完整文案在详情页顶部展示（RecipeDetailPage.healthNotice）
    property int availableCount: 0
    property int missingCount: 0
    property string cartFeedback: ""

    signal clicked()
    signal addMissingToCart()

    // 徽标文字：按文案关键词归为“少盐/控糖/注意”（单字胶囊，不占卡片高度）
    function noticeBadgeText() {
        var n = card.healthNotice
        if (!n) return ""
        if (n.indexOf("糖") >= 0) return "控糖"
        if (n.indexOf("盐") >= 0 || n.indexOf("钠") >= 0) return "少盐"
        return "注意"
    }

    function showCartFeedback(ok) {
        cartFeedback = ok ? "done" : "fail"
        cartFeedbackTimer.restart()
    }

    Timer {
        id: cartFeedbackTimer
        interval: 1000
        onTriggered: cartFeedback = ""
    }

    width: parent ? parent.width : 300
    // 卡高体系（2026-09-05 徽标化方案）：普通 105 / 推荐 140 恒定。健康提示不占卡片高度——
    // 收敛为图片左上角小徽标，完整文案透传到详情页顶部展示，列表高度/图片比例不受提示影响
    height: showMatch ? 140 : 105
    radius: Theme.radiusMedium
    color: Theme.cardBackground

    // 阴影
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 2
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        anchors.bottomMargin: -1
        radius: card.radius
        color: Theme.cardShadowColor
        z: -1
    }

    // 点击交互
    MouseArea {
        anchors.fill: parent
        onClicked: card.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        spacing: Theme.spacingMedium

        // 左侧图片区域
        Rectangle {
            Layout.preferredWidth: 88
            Layout.fillHeight: true
            radius: Theme.radiusSmall
            color: Theme.dividerColor
            clip: true

            Image {
                id: recipeImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                cache: false
                asynchronous: true

                // ── 健康提示徽标（推荐模式；不占卡片高度，与右上匹配度徽章对角） ──
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 3
                    width: healthBadgeText.implicitWidth + 10
                    height: 17
                    radius: 8
                    color: Theme.warningColor
                    opacity: 0.92
                    visible: card.showMatch && card.healthNotice !== ""
                    Text {
                        id: healthBadgeText
                        anchors.centerIn: parent
                        text: card.noticeBadgeText()
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall - 1
                        font.weight: Font.Bold
                        color: Theme.isDarkMode ? "#1F1B16" : "#5D4300"
                    }
                }

                // ── 匹配度徽章（推荐模式） ──
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 3
                    width: badgeText.implicitWidth + 8
                    height: badgeText.implicitHeight + 3
                    radius: 6
                    color: Theme.primaryColor
                    opacity: 0.85
                    visible: card.showMatch && card.matchScore > 0
                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: Math.round(card.matchScore * 100) + "%"
                        font.pointSize: Theme.fontSizeSmall - 2
                        font.weight: Font.Bold
                        color: "#fff"
                        font.family: Theme.fontFamily
                    }
                    // 悬停解释（桌面）：裸百分号易被误读。注意：不用 visible 绑定 MouseArea.hovered
                    // （本环境求值异常会报 Unable to assign [undefined] to bool），改命令式 show/close
                    MouseArea {
                        id: badgeHover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: card.clicked()
                        onEntered: badgeTip.open()
                        onExited: badgeTip.close()
                    }
                    ToolTip {
                        id: badgeTip
                        text: qsTr("与库存和口味的匹配度")
                        delay: 500
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: Theme.dividerColor
                    visible: recipeImage.status === Image.Error || recipeImage.source === ""
                    Canvas {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        property color iconColor: Theme.textHint
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.strokeStyle = iconColor
                            ctx.lineWidth = 1.5
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2 + 1, 7, 0, Math.PI)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(3, height / 2 + 2)
                            ctx.lineTo(width - 3, height / 2 + 2)
                            ctx.stroke()
                        }
                    }
                }
            }

        }

        // 右侧信息区域
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingXSmall

            // 菜谱名称
            Text {
                id: nameLabel
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeH3
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // 简介
            Text {
                id: descLabel
                Layout.fillWidth: true
                font.family: Theme.fontFamily
                font.pointSize: Theme.fontSizeCaption
                color: Theme.textSecondary
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
            }

            // 时间和标签行
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                // 准备时间
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
                        id: prepTimeLabel
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                    }
                }

                // 烹饪时间
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
                        id: cookTimeLabel
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                    }
                }

                Item { Layout.fillWidth: true } // 占位弹性空间

                // 标签列表（动态）
                Flow {
                    Layout.preferredWidth: 80
                    spacing: 2

                    Repeater {
                        id: tagsRepeater
                        delegate: Rectangle {
                            width: tagText.implicitWidth + 8
                            height: 18
                            radius: 9
                            color: Theme.primaryLightColor
                            Text {
                                id: tagText
                                anchors.centerIn: parent
                                text: modelData
                                font.pointSize: Theme.fontSizeSmall - 1
                                color: Theme.textOnPrimary
                            }
                        }
                    }
                }
            }

            // ── 食材匹配条 + 购物车（推荐模式） ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                visible: card.showMatch

                // 匹配条
                Rectangle {
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 4
                    radius: 2
                    color: Theme.dividerColor
                    visible: card.availableCount + card.missingCount > 0
                    Rectangle {
                        width: parent.width * (card.availableCount / Math.max(1, card.availableCount + card.missingCount))
                        height: parent.height
                        radius: 2
                        color: Theme.accentColor
                    }
                }

                Text {
                    text: qsTr("已有%1·缺%2").arg(card.availableCount).arg(card.missingCount)
                    font.pointSize: Theme.fontSizeSmall - 2
                    font.family: Theme.fontFamily
                    color: Theme.textHint
                    visible: card.availableCount + card.missingCount > 0
                }

                Item { Layout.fillWidth: true }

                // 加购按钮
                Rectangle {
                    visible: card.missingCount > 0
                    width: Math.max(cartBtnIdle.implicitWidth, cartBtnDone.implicitWidth) + 14
                    height: 22
                    radius: 4
                    color: card.cartFeedback === "done" ? "#4caf50"
                         : card.cartFeedback === "fail" ? "#f44336"
                         : Theme.primaryColor
                    MouseArea {
                        anchors.fill: parent
                        enabled: card.cartFeedback === ""
                        onClicked: card.addMissingToCart()
                    }
                    Text {
                        id: cartBtnDone
                        visible: false
                        text: "✓"
                        font.pointSize: Theme.fontSizeSmall
                        font.weight: Font.Bold
                        font.family: Theme.fontFamily
                    }
                    Text {
                        id: cartBtnIdle
                        visible: false
                        text: qsTr("+购物车")
                        font.pointSize: Theme.fontSizeSmall - 2
                        font.weight: Font.Bold
                        font.family: Theme.fontFamily
                    }
                    Text {
                        anchors.centerIn: parent
                        text: card.cartFeedback === "done" ? "✓"
                            : card.cartFeedback === "fail" ? "✕"
                            : qsTr("+购物车")
                        font.pointSize: card.cartFeedback === "" ? Theme.fontSizeSmall - 2 : Theme.fontSizeSmall
                        font.weight: Font.Bold
                        font.family: Theme.fontFamily
                        color: "#fff"
                    }
                }
            }
        }
    }
}