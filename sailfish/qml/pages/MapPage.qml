import QtQuick 2.0
import Sailfish.Silica 1.0
import app.comaps 1.0

Page {
    objectName: "mapPage"
    allowedOrientations: Orientation.Portrait
    backNavigation: false

    MapItem {
        anchors.fill: parent
    }

    IconButton {
        id: locateButton
        anchors {
            top: parent.top; topMargin: Theme.paddingLarge * 2
            left: parent.left; leftMargin: Theme.horizontalPageMargin
        }
        // Mirrors location::EMyPositionMode: 0 pending, 1/2 not following, 3/4 following (the
        // latter also rotating with the compass, which the icon doesn't distinguish).
        icon.source: "image://theme/icon-m-gps"
        icon.color: location.mode >= 3 ? Theme.highlightColor : "black"
        onClicked: location.locate()

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Small
            running: location.mode === 0
        }
    }

    IconButton {
        anchors {
            top: parent.top; topMargin: Theme.paddingLarge * 2
            right: downloadButton.left; rightMargin: Theme.paddingLarge
        }
        icon.source: "image://theme/icon-m-search"
        icon.color: "black"
        onClicked: pageStack.push(Qt.resolvedUrl("SearchPage.qml"))
    }

    IconButton {
        id: downloadButton
        anchors {
            top: parent.top; topMargin: Theme.paddingLarge * 2
            right: parent.right; rightMargin: Theme.horizontalPageMargin
        }
        icon.source: "image://theme/icon-m-cloud-download"
        // The map's light background would swallow the default light icon.
        icon.color: "black"
        onClicked: pageStack.push(Qt.resolvedUrl("DownloaderPage.qml"))
    }

    PlacePanel {
        id: placePanel
    }
}
