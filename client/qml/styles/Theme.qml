pragma Singleton
import QtQuick

QtObject {
    // ========== 颜色系统 ==========
    // 主色调（橙色系，代表食欲与温暖）
    readonly property color primaryColor: "#FF6B35"
    readonly property color primaryLightColor: "#FF8C5A"
    readonly property color primaryDarkColor: "#E55A2B"

    // 辅助色
    readonly property color accentColor: "#4CAF50"      // 绿色，代表健康
    readonly property color warningColor: "#FFC107"
    readonly property color errorColor: "#F44336"

    // 中性色（文字与背景）
    readonly property color textPrimary: "#212121"
    readonly property color textSecondary: "#757575"
    readonly property color textHint: "#9E9E9E"
    readonly property color dividerColor: "#E0E0E0"
    readonly property color backgroundColor: "#F5F5F5"
    readonly property color cardBackground: "#FFFFFF"

    // ========== 字体系统 ==========
    readonly property string fontFamily: "Microsoft YaHei, PingFang SC, Helvetica Neue, Arial, sans-serif"

    // 字体大小（基于 16px 基准）
    readonly property int fontSizeH1: 24
    readonly property int fontSizeH2: 20
    readonly property int fontSizeH3: 18
    readonly property int fontSizeBody: 15
    readonly property int fontSizeCaption: 13
    readonly property int fontSizeSmall: 12

    // 字体粗细
    readonly property int fontWeightLight: Font.Light
    readonly property int fontWeightNormal: Font.Normal
    readonly property int fontWeightMedium: Font.Medium
    readonly property int fontWeightBold: Font.Bold

    // ========== 间距系统 ==========
    readonly property int spacingXSmall: 4
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 16
    readonly property int spacingLarge: 24
    readonly property int spacingXLarge: 32

    // ========== 圆角 ==========
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int radiusXLarge: 16

    // ========== 动画时长 ==========
    readonly property int durationShort: 150
    readonly property int durationMedium: 300
    readonly property int durationLong: 500
}