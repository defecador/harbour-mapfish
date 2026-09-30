import QtQuick 2.0
import Sailfish.Silica 1.0
import app.mapfish 1.0

Page {
    id: page

    property string parentId: ""

    allowedOrientations: Orientation.Portrait

    function formatSize(bytes) {
        if (bytes >= 1024 * 1024 * 1024)
            return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
        if (bytes >= 1024 * 1024)
            return Math.round(bytes / (1024 * 1024)) + " MB"
        return Math.max(1, Math.round(bytes / 1024)) + " kB"
    }

    function statusText(status, size) {
        switch (status) {
        case "downloaded": return qsTr("Downloaded")
        case "partly": return qsTr("Partly downloaded")
        case "outdated": return qsTr("Update available")
        case "queued": return qsTr("Waiting")
        case "error": return qsTr("Download failed")
        default: return formatSize(size)
        }
    }

    CountryModel {
        id: countries
        parentId: page.parentId
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: countries

        header: PageHeader {
            title: page.parentId === "" ? qsTr("Download maps") : countries.parentName
        }

        delegate: ListItem {
            id: item

            property bool busy: model.status === "downloading" || model.status === "queued"
            property bool onDisk: model.status === "downloaded" || model.status === "partly"
                                  || model.status === "outdated"

            contentHeight: Theme.itemSizeMedium
            menu: busy || onDisk ? contextMenu : null

            onClicked: {
                if (model.expandable)
                    pageStack.push(Qt.resolvedUrl("DownloaderPage.qml"), { parentId: model.countryId })
                else if (onDisk) {
                    countries.show(model.countryId)
                    pageStack.pop(pageStack.find(function(p) { return p.objectName === "mapPage" }))
                } else if (!busy)
                    countries.download(model.countryId)
            }

            Label {
                id: nameLabel
                anchors {
                    left: parent.left; leftMargin: Theme.horizontalPageMargin
                    right: action.left; rightMargin: Theme.paddingMedium
                    top: parent.top; topMargin: Theme.paddingSmall
                }
                text: model.name
                truncationMode: TruncationMode.Fade
                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
            }

            Label {
                anchors {
                    left: nameLabel.left; right: nameLabel.right
                    top: nameLabel.bottom
                }
                visible: !item.busy
                text: page.statusText(model.status, model.size)
                font.pixelSize: Theme.fontSizeExtraSmall
                color: model.status === "error" ? Theme.errorColor : Theme.secondaryColor
            }

            ProgressBar {
                anchors {
                    left: parent.left; right: action.left
                    top: nameLabel.bottom; topMargin: -Theme.paddingLarge
                }
                visible: item.busy
                indeterminate: model.status === "queued"
                value: model.progress
                label: model.status === "queued" ? qsTr("Waiting") : Math.round(model.progress * 100) + " %"
            }

            Icon {
                id: action
                anchors {
                    right: parent.right; rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                source: model.expandable ? "image://theme/icon-m-right"
                      : item.onDisk ? "image://theme/icon-m-location"
                      : item.busy ? "" : "image://theme/icon-m-cloud-download"
            }

            Component {
                id: contextMenu
                ContextMenu {
                    MenuItem {
                        visible: item.busy
                        text: qsTr("Cancel download")
                        onClicked: countries.cancel(model.countryId)
                    }
                    MenuItem {
                        visible: model.status === "outdated"
                        text: qsTr("Update")
                        onClicked: countries.download(model.countryId)
                    }
                    MenuItem {
                        visible: item.onDisk
                        text: qsTr("Delete")
                        onClicked: {
                            var id = model.countryId
                            item.remorseDelete(function() { countries.remove(id) })
                        }
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
