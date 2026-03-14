import QtQuick
import QmltypesSingletonModule 1.0

Item {
    property string value: MetaSingleton.env()
}
