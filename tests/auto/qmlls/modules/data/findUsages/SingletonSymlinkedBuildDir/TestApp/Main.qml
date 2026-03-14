import QtQuick
import qs.Commons
import qs.Commons as QC

Item {
    property string shortcut: Keybinds.shortcut
    property string qualifiedShortcut: QC.Keybinds.shortcut
    property string direct: Keybinds.answer("Qt")
    property string qualified: QC.Keybinds.answer("Qt")
}
