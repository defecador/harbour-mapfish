import QtQuick 2.0
import Sailfish.Silica 1.0
import app.comaps 1.0

Page {
    allowedOrientations: Orientation.Portrait
    backNavigation: false

    MapItem {
        anchors.fill: parent
    }
}
