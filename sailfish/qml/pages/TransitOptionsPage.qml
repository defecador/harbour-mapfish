import QtQuick 2.0
import Sailfish.Silica 1.0

// Filters and sort order for transitRoute. A separate page rather than a MapPage PullDownMenu:
// the map already consumes vertical drags for panning, so a pull-down gesture there just opens
// Sailfish's own top-edge (events) shortcut instead of the app's menu.
Page {
    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader { title: qsTr("Public transport") }

            ComboBox {
                width: parent.width
                label: qsTr("Sort by")
                currentIndex: transitRoute.sort
                menu: ContextMenu {
                    MenuItem { text: qsTr("Fastest") }
                    MenuItem { text: qsTr("Fewest changes") }
                    MenuItem { text: qsTr("Least walking") }
                }
                onCurrentIndexChanged: transitRoute.sort = currentIndex
            }

            SectionHeader { text: qsTr("Avoid") }

            TextSwitch {
                text: qsTr("Trams")
                checked: transitRoute.excludeTram
                onCheckedChanged: transitRoute.excludeTram = checked
            }
            TextSwitch {
                text: qsTr("Buses")
                checked: transitRoute.excludeBus
                onCheckedChanged: transitRoute.excludeBus = checked
            }
            TextSwitch {
                text: qsTr("Subway")
                checked: transitRoute.excludeSubway
                onCheckedChanged: transitRoute.excludeSubway = checked
            }
            TextSwitch {
                text: qsTr("Trains")
                description: qsTr("S-Bahn, regional and long-distance trains")
                checked: transitRoute.excludeRail
                onCheckedChanged: transitRoute.excludeRail = checked
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Changes here apply the next time you tap \"Transit here\" on a place.")
            }
        }
    }
}
