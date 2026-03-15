import QtQuick

Item {
    id: root

    property int count: 1
    property int total: 0
    signal fired(message: string)

    property Component lazyComponent: Component {
        NeighborType {
            extra: 1
        }
    }

    function updateLabel(message: string): void {
        total = message.length
    }

    NeighborType {
        id: helper
        extra: Qt.binding(function() {
            return root.count
        })
    }

    Component.onCompleted: {
        fired.connect(updateLabel)
        total = Qt.binding(function() {
            return helper.extra + root.count
        })
        fired.disconnect(updateLabel)
        fired("ready")
    }

    Loader {
        sourceComponent: root.lazyComponent
    }
}
