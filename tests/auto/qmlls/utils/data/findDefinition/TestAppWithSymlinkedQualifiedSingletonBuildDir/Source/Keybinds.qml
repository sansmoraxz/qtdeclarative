pragma Singleton

import QtQml

QtObject {
    readonly property string shortcut: "Ctrl+X"

    function answer(prefix) {
        return prefix + "!"
    }
}
