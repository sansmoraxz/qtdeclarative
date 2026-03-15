import QtQuick
import ObjectBindingSingletonModule

Item {
    property bool active: Settings.data.wallpaper.enabled
}
