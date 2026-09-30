import QtQuick 2.0
import Sailfish.Silica 1.0
import app.mapfish 1.0

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
        id: transitLayerButton
        anchors {
            top: locateButton.bottom; topMargin: Theme.paddingMedium
            left: parent.left; leftMargin: Theme.horizontalPageMargin
        }
        icon.source: "image://theme/icon-m-train"
        icon.color: layers.transit ? Theme.highlightColor : "black"
        onClicked: layers.transit = !layers.transit
    }

    IconButton {
        anchors {
            top: transitLayerButton.bottom; topMargin: Theme.paddingMedium
            left: parent.left; leftMargin: Theme.horizontalPageMargin
        }
        icon.source: "image://theme/icon-m-setting"
        icon.color: "black"
        onClicked: pageStack.push(Qt.resolvedUrl("TransitOptionsPage.qml"))
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

    Rectangle {
        id: routeBar
        anchors {
            // Sits above the place panel's own buttons instead of covering them while it's open.
            bottom: placePanel.open ? placePanel.top : parent.bottom
            left: parent.left
            right: parent.right
        }
        height: routeRow.height + 2 * Theme.paddingMedium
        color: Theme.overlayBackgroundColor
        opacity: 0.95
        visible: route.active || route.building || route.error !== ""

        Row {
            id: routeRow
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left; leftMargin: Theme.horizontalPageMargin
                right: routeControls.left; rightMargin: Theme.paddingSmall
            }
            spacing: Theme.paddingMedium

            BusyIndicator {
                size: BusyIndicatorSize.Small
                running: route.building
                visible: route.building
            }
            Label {
                width: parent.width - (route.building ? parent.spacing + height : 0)
                text: route.active ? (route.mode === 1 ? qsTr("Bike: %1, %2") : qsTr("Walk: %1, %2"))
                                      .arg(route.distance).arg(route.time)
                                    : route.error
                wrapMode: Text.Wrap
                color: route.error !== "" && !route.active ? Theme.errorColor : Theme.primaryColor
            }
        }

        Row {
            id: routeControls
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right; rightMargin: Theme.paddingSmall
            }

            Button {
                preferredWidth: Theme.buttonWidthSmall
                // No cycling icon in the theme; switching to the other mode is rare enough that a
                // text label reads fine next to the clear button.
                text: route.mode === 1 ? qsTr("Walk") : qsTr("Bike")
                onClicked: route.mode = route.mode === 1 ? 0 : 1
            }
            IconButton {
                icon.source: "image://theme/icon-m-clear"
                onClicked: route.clear()
            }
        }
    }

    Rectangle {
        id: transitBar
        anchors {
            // Stacks above the walk/bike bar and the place panel, so nothing overlaps.
            bottom: routeBar.visible ? routeBar.top : (placePanel.open ? placePanel.top : parent.bottom)
            left: parent.left
            right: parent.right
        }
        height: transitColumn.height + 2 * Theme.paddingMedium
        color: Theme.overlayBackgroundColor
        opacity: 0.95
        visible: transitRoute.active || transitRoute.building || transitRoute.error !== ""

        Column {
            id: transitColumn
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left; leftMargin: Theme.horizontalPageMargin
                right: clearTransitButton.left; rightMargin: Theme.paddingSmall
            }
            spacing: Theme.paddingSmall

            Row {
                width: parent.width
                spacing: Theme.paddingMedium

                BusyIndicator {
                    size: BusyIndicatorSize.Small
                    running: transitRoute.building
                    visible: transitRoute.building
                }
                Label {
                    width: parent.width - (transitRoute.building ? parent.spacing + height : 0)
                    text: transitRoute.active ? transitRoute.summary : transitRoute.error
                    wrapMode: Text.Wrap
                    color: transitRoute.error !== "" && !transitRoute.active ? Theme.errorColor : Theme.primaryColor
                }
            }
            Label {
                width: parent.width
                visible: transitRoute.active
                text: transitRoute.legs
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
            }
        }

        IconButton {
            id: clearTransitButton
            anchors {
                top: parent.top; topMargin: Theme.paddingSmall
                right: parent.right; rightMargin: Theme.paddingSmall
            }
            icon.source: "image://theme/icon-m-clear"
            onClicked: transitRoute.clear()
        }
    }
}
