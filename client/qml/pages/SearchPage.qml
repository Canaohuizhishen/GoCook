import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
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
                    font.pointSize: 19
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

            // 搜索输入框
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: Theme.searchBarBackground
                radius: Theme.radiusLarge
                border.width: searchField.activeFocus ? 1 : 0
                border.color: Theme.primaryColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 6
                    spacing: 8

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        topPadding: 6
                        bottomPadding: 6
                        background: null
                        font.pointSize: Theme.fontSizeBody
                        color: Theme.textPrimary
                        placeholderText: qsTr("搜索菜谱、食材...")
                        placeholderTextColor: Theme.textHint
                        verticalAlignment: TextInput.AlignVCenter
                        selectByMouse: true

                        onAccepted: searchPage.doSearch()

                        onTextChanged: {
                            if (searchField.text.trim() === "") {
                                recipeVM.resetSearch()
                            }
                        }
                    }

                    RoundButton {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        radius: 14
                        flat: true
                        visible: searchField.text.trim() !== ""

                        contentItem: Text {
                            text: "\u00D7"
                            font.pointSize: 14
                            color: Theme.textHint
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            searchField.clear()
                        }
                    }
                }
            }

            ToolButton {
                text: qsTr("搜索")
                font.pointSize: Theme.fontSizeCaption
                leftPadding: 6
                rightPadding: 6
                enabled: searchField.text.trim() !== ""
                onClicked: searchPage.doSearch()

                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.enabled ? Theme.primaryColor : Theme.textHint
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // 内容区（固定占满剩余空间，子项互切不回流）
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            LoadingIndicator {
                anchors.centerIn: parent
                fullscreen: false
                message: qsTr("正在搜索...")
                isLoading: recipeVM.searchLoading
                visible: recipeVM.searchLoading
            }

            ListView {
                id: resultListView
                anchors.fill: parent
                leftMargin: Theme.spacingMedium
                rightMargin: Theme.spacingMedium
                spacing: Theme.spacingSmall
                clip: true
                visible: recipeVM.searchResults.length > 0

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                model: recipeVM.searchResults

                delegate: RecipeCard {
                    width: resultListView.width - resultListView.leftMargin - resultListView.rightMargin
                    recipeName: modelData.name
                    recipeDescription: modelData.description
                    imageSource: modelData.imageUrl ? authViewModel.apiBaseUrl + modelData.imageUrl : ""
                    prepTime: modelData.prepTime + qsTr("分钟")
                    cookTime: modelData.cookTime + qsTr("分钟")
                    tags: modelData.tags || []

                    onClicked: {
                        searchPage.recipeClicked(modelData.id)
                    }
                }

                onAtYEndChanged: {
                    if (atYEnd && !recipeVM.searchLoading && recipeVM.searchHasMore) {
                        recipeVM.searchNextPage()
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: Theme.spacingMedium
                visible: recipeVM.searchPerformed
                         && !recipeVM.searchLoading
                         && recipeVM.searchResults.length === 0

                Canvas {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 36
                    height: 36
                    property color iconColor: Theme.textHint
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.strokeStyle = iconColor
                        ctx.lineWidth = 2.5
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.ellipse(4, 4, 20, 20)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(20.5, 20.5)
                        ctx.lineTo(31, 31)
                        ctx.stroke()
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("暂无匹配结果")
                    color: Theme.textHint
                    font.pointSize: Theme.fontSizeBody
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("试试其他关键词吧")
                    color: Theme.textHint
                    font.pointSize: Theme.fontSizeCaption
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
        font.pointSize: Theme.fontSizeCaption
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
