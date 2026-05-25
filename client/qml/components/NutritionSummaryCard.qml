import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "."

Column {
    id: root
    width: parent ? parent.width : 200
    spacing: Theme.spacingXSmall
    visible: _nutrition && _nutrition.calories > 0

    property int detailRecipeId: 0
    property var stackView: null
    readonly property var _nutrition: recipeVM.recipeDetail.nutrition || ({})

    SectionHeader {
        headerText: qsTr("营养信息")
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
