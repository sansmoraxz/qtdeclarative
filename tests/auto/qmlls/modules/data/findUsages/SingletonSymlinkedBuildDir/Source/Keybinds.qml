pragma Singleton

import QtQml

QtObject {
    function answer(prefix) {
        return prefix + "!"
    }
}
