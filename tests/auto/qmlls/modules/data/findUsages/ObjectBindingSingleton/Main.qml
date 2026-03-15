import QtQuick
import "ObjectBindingSingletonModule" as Test

Item {
    property bool active: Test.Settings.data.wallpaper.enabled
}
