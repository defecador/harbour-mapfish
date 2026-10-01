import QtQuick 2.0
import Sailfish.Silica 1.0
import app.mapfish 1.0

Page {
    id: page

    allowedOrientations: Orientation.Portrait

    SearchModel {
        id: results
        // A new search replaces the markers of the previous one.
        Component.onCompleted: clearMarks()
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: results
        currentIndex: -1

        PullDownMenu {
            visible: results.showingHistory && list.count > 0
            MenuItem {
                text: qsTr("Clear recent searches")
                onClicked: results.clearHistory()
            }
        }

        header: Column {
            width: list.width

            PageHeader {
                title: qsTr("Search")
            }

            SearchField {
                id: field
                width: parent.width
                placeholderText: qsTr("Places, addresses, categories")
                inputMethodHints: Qt.ImhNoPredictiveText
                onTextChanged: results.query = text
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
                Component.onCompleted: forceActiveFocus()

                Connections {
                    target: results
                    onQueryChanged: if (field.text !== results.query) field.text = results.query
                }
            }
        }

        delegate: ListItem {
            contentHeight: Theme.itemSizeMedium

            onClicked: {
                if (model.suggestion !== "") {
                    results.query = model.suggestion
                    return
                }
                results.show(index)
                pageStack.pop()
            }

            Label {
                id: nameLabel
                anchors {
                    left: parent.left; leftMargin: Theme.horizontalPageMargin
                    right: parent.right; rightMargin: Theme.horizontalPageMargin
                    top: parent.top; topMargin: Theme.paddingSmall
                }
                text: model.name
                truncationMode: TruncationMode.Fade
                color: highlighted ? Theme.highlightColor : Theme.primaryColor
            }

            Label {
                anchors {
                    left: nameLabel.left; right: nameLabel.right
                    top: nameLabel.bottom
                }
                text: [model.type, model.address].filter(function(s) { return s !== "" }).join(" · ")
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeExtraSmall
                color: highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !results.busy && !results.showingHistory
            text: qsTr("Nothing found")
            hintText: qsTr("Search covers the maps you have downloaded")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: results.busy && list.count === 0
        }

        VerticalScrollDecorator {}
    }
}
