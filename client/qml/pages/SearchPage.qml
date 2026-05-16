import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client.styles
import "../components"

Page {
    id: searchPage
    title: qsTr("搜索")

    signal recipeClicked(int recipeId)

    // 页面加载后立刻聚焦搜索框
    Component.onCompleted: {
        searchField.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶部栏：返回 + 搜索框 + 搜索按钮
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingMedium
            Layout.topMargin: Theme.spacingMedium
            spacing: Theme.spacingSmall

            // 返回按钮
            RoundButton {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: 18
                flat: true

                contentItem: Text {
                    text: "\u2039"
                    font.pixelSize: 26
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    recipeVM.resetSearch()
                    var sv = searchPage._stackView
                    if (sv) sv.pop()
                }
            }

            // 搜索输入框（搜索按钮在内部右侧）
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                color: Theme.isDarkMode ? "#2A2A2A" : "#EEEEEE"
                radius: Theme.radiusLarge
                border.width: searchField.activeFocus ? 1 : 0
                border.color: Theme.primaryColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 6
                    spacing: 0

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        padding: 0
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        placeholderText: qsTr("搜索菜谱、食材...")
                        placeholderTextColor: Theme.textHint
                        verticalAlignment: TextInput.AlignVCenter
                        background: Item {}
                        selectByMouse: true

                        onAccepted: searchPage.doSearch()
                    }

                    // 搜索按钮（在搜索框内部右侧）
                    RoundButton {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 17
                        flat: false

                        background: Rectangle {
                            radius: 17
                            color: searchField.text.trim() !== ""
                                   ? Theme.primaryColor
                                   : Theme.textHint
                            opacity: searchField.text.trim() !== "" ? 1.0 : 0.5
                        }

                        contentItem: Text {
                            text: "\u2192"
                            font.pixelSize: 16
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: searchPage.doSearch()
                    }
                }
            }
        }

        // 加载指示器
        LoadingIndicator {
            id: loadingIndicator
            fullscreen: false
            message: qsTr("正在搜索...")
            isLoading: recipeVM.searchLoading
            visible: recipeVM.searchLoading
        }

        // 结果列表
        ListView {
            id: resultListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingSmall
            clip: true
            visible: !loadingIndicator.isLoading
            leftMargin: Theme.spacingMedium
            rightMargin: Theme.spacingMedium

            model: recipeVM.searchResults

            delegate: RecipeCard {
                width: resultListView.width - resultListView.leftMargin - resultListView.rightMargin
                recipeName: modelData.name
                recipeDescription: modelData.description
                imageSource: modelData.imageUrl || ""
                prepTime: modelData.prepTime + qsTr("分钟")
                cookTime: modelData.cookTime + qsTr("分钟")
                tags: modelData.tags || []
                isFavorite: modelData.isFavorite || false

                onClicked: {
                    searchPage.recipeClicked(modelData.id)
                }

                onFavoriteClicked: {
                    console.log("Toggle favorite for:", modelData.id)
                }
            }

            footer: Item {
                width: resultListView.width
                height: recipeVM.searchHasMore ? 50 : 0
                visible: recipeVM.searchHasMore

                CustomButton {
                    anchors.centerIn: parent
                    buttonText: qsTr("加载更多")
                    buttonType: CustomButton.ButtonType.Secondary
                    onClicked: recipeVM.searchNextPage()
                }
            }
        }

        // 空状态提示
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: recipeVM.searchPerformed
                     && !recipeVM.searchLoading
                     && recipeVM.searchResults.length === 0

            Column {
                anchors.centerIn: parent
                spacing: Theme.spacingMedium

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\uD83D\uDD0D"  // 🔍
                    font.pixelSize: 48
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("暂无匹配结果")
                    color: Theme.textHint
                    font.pixelSize: Theme.fontSizeBody
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("试试其他关键词吧")
                    color: Theme.textHint
                    font.pixelSize: Theme.fontSizeCaption
                    opacity: 0.6
                }
            }
        }
    }

    // 错误提示
    Label {
        id: errorLabel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacingLarge
        text: ""
        color: Theme.errorColor
        font.pixelSize: Theme.fontSizeCaption
        visible: text !== ""
    }

    Connections {
        target: recipeVM
        function onSearchErrorOccurred(error) {
            errorLabel.text = error
        }
    }

    function doSearch() {
        var kw = searchField.text.trim()
        if (kw === "") return
        errorLabel.text = ""
        recipeVM.searchRecipes(kw)
    }
}
