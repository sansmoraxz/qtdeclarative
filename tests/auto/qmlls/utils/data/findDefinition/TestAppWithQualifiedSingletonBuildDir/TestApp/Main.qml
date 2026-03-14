import QtQuick
import qs.Commons
import qs.Commons as QC

Item {
    property string direct: Keybinds.answer("Qt")
    property string qualified: QC.Keybinds.answer("Qt")
    property string legacyDirect: Keybinds.legacyAnswer("Qt")
    property string legacyQualified: QC.Keybinds.legacyAnswer("Qt")
}
