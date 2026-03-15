pragma Singleton

import QtQml

QtObject {
    id: root

    readonly property alias data: adapter

    QtObject {
        id: adapter

        property Base wallpaper: Base {
            property bool enabled: true
        }
    }
}
