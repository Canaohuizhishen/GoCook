import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
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
    property alias fontSize: buttonText.font.pixelSize

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
                if (control.pressed) return Theme.primaryDarkColor
                if (control.hovered) return Theme.primaryLightColor
                return Theme.primaryColor
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
        layer.enabled: control.buttonType === CustomButton.ButtonType.Primary && control.enabled && !control.pressed
        layer.effect: DropShadow {
            verticalOffset: 2
            radius: 8
            samples: 17
            color: "#20000000"
        }

        Behavior on color {
            ColorAnimation { duration: Theme.durationShort }
        }
    }

    // 文字
    contentItem: Text {
        id: buttonText
        text: control.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
        font.weight: control.buttonType === CustomButton.ButtonType.Primary ? Theme.fontWeightMedium : Theme.fontWeightNormal
        color: {
            if (!control.enabled) return Theme.textHint
            if (control.buttonType === CustomButton.ButtonType.Primary) return "white"
            if (control.buttonType === CustomButton.ButtonType.Text) {
                if (control.hovered) return Theme.primaryColor
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