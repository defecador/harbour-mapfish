import QtQuick 2.0
import Sailfish.Silica 1.0

// Where the trip starts from. Tapping it opens search to pick another place; the cross goes back
// to the device's position.
BackgroundItem {
    height: Theme.itemSizeExtraSmall
    onClicked: pageStack.push(Qt.resolvedUrl("SearchPage.qml"), { pickOrigin: true })

    Label {
        id: fromLabel
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("From")
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }
    Label {
        anchors {
            verticalCenter: parent.verticalCenter
            left: fromLabel.right; leftMargin: Theme.paddingMedium
            right: icons.left; rightMargin: Theme.paddingSmall
        }
        text: origin.custom ? origin.name : qsTr("My position")
        truncationMode: TruncationMode.Fade
        color: origin.custom ? Theme.highlightColor : Theme.primaryColor
    }
    Row {
        id: icons
        anchors { verticalCenter: parent.verticalCenter; right: parent.right }
        IconButton {
            visible: origin.custom
            width: Theme.itemSizeExtraSmall; height: width
            icon.source: "image://theme/icon-s-clear-opaque-cross"
            onClicked: origin.reset()
        }
        Image {
            anchors.verticalCenter: parent.verticalCenter
            source: "image://theme/icon-s-edit"
        }
    }
}
