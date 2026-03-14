import QtQuick
import QmltypesSingletonModule 1.0

Item {
    property string value: MetaSingleton.env("PATH")
    property string currentVersion: MetaSingleton.version
    property var versionChangedSignal: MetaSingleton.versionChanged
    property var failureSignal: MetaSingleton.reloadFailed
}
