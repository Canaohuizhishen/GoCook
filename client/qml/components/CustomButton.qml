import QtQuick
import QtQuick.Controls

import client.styles

Button {
    id: control

    // 按钮类型枚举
    enum ButtonType {
        Primary,    // 主要操作（填充主色）
        Secondary,  // 次要操作（边框）
        Text        // 纯文字按钮
    }

    // 可配置属性
    property int buttonType: CustomButton.ButtonType.Primary
    property alias buttonText: buttonText.text
    property alias textColor: buttonText.color
    property alias fontSize: buttonText.font.pointSize
    property color buttonColor: "transparent"

    // 基础样式
    implicitWidth: 120
    implicitHeight: 44
    flat: true
    hoverEnabled: true

    // 背景
    background: Rectangle {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight
        radius: Theme.radiusMedium
        color: {
            if (!control.enabled) return Theme.dividerColor
            if (control.buttonType === CustomButton.ButtonType.Primary) {
                if (control.pressed) return control.buttonColor.a > 0 ? Qt.darker(control.buttonColor) : Theme.primaryDarkColor
                if (control.hovered) return control.buttonColor.a > 0 ? Qt.lighter(control.buttonColor) : Theme.primaryLightColor
                return control.buttonColor.a > 0 ? control.buttonColor : Theme.primaryColor
            }
            return "transparent"
        }
        border.color: {
            if (control.buttonType === CustomButton.ButtonType.Secondary) {
                return Theme.primaryColor
            }
            return "transparent"
        }
        border.width: control.buttonType === CustomButton.ButtonType.Secondary ? 1 : 0

        // 阴影效果（仅主要按钮）

        Behavior on color {
            ColorAnimation { duration: Theme.durationShort }
        }
    }

    // 文字
    contentItem: Text {
        id: buttonText
        text: control.text
        font.family: Theme.fontFamily
        font.pointSize: Theme.fontSizeBody
        font.weight: control.buttonType === CustomButton.ButtonType.Primary ? Theme.fontWeightMedium : Theme.fontWeightNormal
        color: {
            if (!control.enabled) return Theme.textHint
            if (control.buttonType === CustomButton.ButtonType.Primary) return Theme.textOnPrimary
            if (control.buttonType === CustomButton.ButtonType.Text) {
                return Theme.primaryColor
            }
            return Theme.primaryColor
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        Behavior on color {
            ColorAnimation { duration: Theme.durationShort }
        }
    }

    // 点击波纹效果（仅 Material 风格，可选）
    // 若需要，可添加 Ripple 组件
}