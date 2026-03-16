import QtQuick
import QmltypesAliasedSingletonModule 1.0

Item {
    property string aliasedValue: AliasedSingleton.env("PATH")
}
