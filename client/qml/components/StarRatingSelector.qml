import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import client
import "."

RowLayout {
    id: root
    spacing: 4

    property int rating: 0
    property int starCount: 5
    property real starSize: Theme.fontSizeH2
    property bool readOnly: false
    property color activeColor: Theme.warningColor
    property color inactiveColor: Theme.textHint

    signal ratingModified(int newRating)

    Repeater {
        model: root.starCount
        Text {
            text: (index < root.rating) ? "\u2605" : "\u2606"
            font.pointSize: root.starSize
            color: (index < root.rating) ? root.activeColor : root.inactiveColor

            MouseArea {
                anchors.fill: parent
                enabled: !root.readOnly
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.rating = index + 1
                    root.ratingModified(root.rating)
                }
            }
        }
    }
}
