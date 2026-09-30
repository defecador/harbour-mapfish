import QtQuick 2.0
import Sailfish.Silica 1.0

// Details of what is selected on the map. Swiping it away deselects the object.
DockedPanel {
    id: panel

    width: parent.width
    height: Math.min(content.height + 2 * Theme.paddingLarge, parent.height * 0.6)
    dock: Dock.Bottom
    modal: false

    Connections {
        target: placePage
        onChanged: {
            if (placePage.open)
                panel.show()
            else
                panel.hide()
        }
    }
    onOpenChanged: if (!open && placePage.open) placePage.close()

    Rectangle {
        anchors.fill: parent
        color: Theme.overlayBackgroundColor
        opacity: 0.9
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + 2 * Theme.paddingLarge
        clip: true

        Column {
            id: content
            y: Theme.paddingLarge
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            spacing: Theme.paddingSmall

            Label {
                width: parent.width
                text: placePage.title
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
            }
            Label {
                width: parent.width
                visible: text !== ""
                text: placePage.subtitle
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
            }
            Label {
                width: parent.width
                visible: text !== ""
                text: placePage.address
                wrapMode: Text.Wrap
            }

            DetailItem { label: qsTr("Opening hours"); value: placePage.openingHours; visible: value !== "" }

            BackgroundItem {
                width: parent.width
                height: phoneDetail.height
                visible: placePage.phone !== ""
                onClicked: Qt.openUrlExternally("tel:" + placePage.phone)
                DetailItem { id: phoneDetail; label: qsTr("Phone"); value: placePage.phone }
            }
            BackgroundItem {
                width: parent.width
                height: websiteDetail.height
                visible: placePage.website !== ""
                onClicked: Qt.openUrlExternally(placePage.website.indexOf("://") < 0
                                                ? "https://" + placePage.website : placePage.website)
                DetailItem { id: websiteDetail; label: qsTr("Website"); value: placePage.website }
            }
            BackgroundItem {
                width: parent.width
                height: emailDetail.height
                visible: placePage.email !== ""
                onClicked: Qt.openUrlExternally("mailto:" + placePage.email)
                DetailItem { id: emailDetail; label: qsTr("Email"); value: placePage.email }
            }

            DetailItem { label: qsTr("Coordinates"); value: placePage.coordinates }

            Label {
                width: parent.width
                visible: text !== ""
                text: placePage.description
                textFormat: Text.StyledText
                wrapMode: Text.Wrap
                maximumLineCount: 8
                truncationMode: TruncationMode.Elide
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
            }
        }

        VerticalScrollDecorator {}
    }
}
