pragma Singleton
import QtQuick

QtObject {
    // ========== 主题模式 ==========
    readonly property int themeModeSystem: 0
    readonly property int themeModeLight: 1
    readonly property int themeModeDark: 2

    property int themeMode: 0

    readonly property bool systemIsDark: Qt.styleHints.colorScheme === Qt.Dark

    readonly property bool isDarkMode:
        themeMode === themeModeDark ? true :
        themeMode === themeModeLight ? false :
        systemIsDark

    // ========== 颜色系统 ==========
    // 主色调（橙色系，代表食欲与温暖）
    readonly property color primaryColor: "#FF6B35"
    readonly property color primaryLightColor: "#FF8C5A"
    readonly property color primaryDarkColor: "#E55A2B"

    // 辅助色
    readonly property color accentColor: "#4CAF50"
    readonly property color warningColor: "#FFC107"
    readonly property color errorColor: "#F44336"

    // 中性色（文字与背景，根据主题切换）
    readonly property color textPrimary: isDarkMode ? "#E0E0E0" : "#212121"
    readonly property color textSecondary: isDarkMode ? "#A0A0A0" : "#757575"
    readonly property color textHint: isDarkMode ? "#707070" : "#9E9E9E"
    readonly property color dividerColor: isDarkMode ? "#3A3A3A" : "#E0E0E0"
    readonly property color backgroundColor: isDarkMode ? "#121212" : "#F5F5F5"
    readonly property color cardBackground: isDarkMode ? "#1E1E1E" : "#FFFFFF"

    readonly property color cardShadowColor: isDarkMode ? Qt.rgba(0, 0, 0, 0.20) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color textOnPrimary: "#FFFFFF"
    readonly property color searchBarBackground: isDarkMode ? "#2A2A2A" : "#EEEEEE"

    // ========== 字体系统 ==========
    readonly property string fontFamily: "Microsoft YaHei, PingFang SC, Helvetica Neue, Arial, sans-serif"

    readonly property int fontSizeH1: 18
    readonly property int fontSizeH2: 16
    readonly property int fontSizeH3: 14
    readonly property int fontSizeBody: 13
    readonly property int fontSizeCaption: 11
    readonly property int fontSizeSmall: 10

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

    // ========== 布局常量 ==========
    readonly property int tabIconSize: 22
    readonly property int gridSpacing: 8

    // ========== 动画时长 ==========
    readonly property int durationShort: 150
    readonly property int durationMedium: 300
    readonly property int durationLong: 500
}
