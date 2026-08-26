import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "."

Column {
    id: root
    width: parent ? parent.width : 200
    spacing: Theme.spacingXSmall
    // 有数据判定：优先用服务端 has_data；旧缓存/旧服务端缺字段时回退 calories>0
    readonly property bool _hasNutrition: !!_nutrition && _nutrition !== null
        && (_nutrition.has_data !== undefined ? _nutrition.has_data : _nutrition.calories > 0)
    visible: _hasNutrition

    property int detailRecipeId: 0
    property var stackView: null
    // 是否显示自带标题（编辑页外层已有标题时传 false，避免重复）
    property bool showHeader: true
    readonly property var _nutrition: recipeVM.recipeDetail.nutrition || ({})

    SectionHeader {
        headerText: qsTr("营养合计")
        visible: root.showHeader
    }

    Grid {
        columns: 2
        width: parent.width
        spacing: 4

        Text { text: qsTr("热量"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeCaption }
        Text { text: (_nutrition.calories || 0) + " kcal"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeCaption }
        Text { text: qsTr("蛋白质"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeCaption }
        Text { text: (_nutrition.protein || 0) + " g"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeCaption }
        Text { text: qsTr("脂肪"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeCaption }
        Text { text: (_nutrition.fat || 0) + " g"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeCaption }
        Text { text: qsTr("碳水"); color: Theme.textSecondary; font.pointSize: Theme.fontSizeCaption }
        Text { text: (_nutrition.carbs || 0) + " g"; color: Theme.textPrimary; font.pointSize: Theme.fontSizeCaption }
    }

    // 摘要与按钮之间的间距（避免紧挨）
    Item { width: parent.width; height: Theme.spacingMedium }

    CustomButton {
        width: parent.width
        buttonText: "\u2139 " + qsTr("查看详细营养报告")
        buttonType: CustomButton.ButtonType.Secondary
        onClicked: {
            if (stackView)
                stackView.push("../pages/NutritionReportPage.qml", {recipeId: detailRecipeId, _stackView: stackView})
        }
    }
}
