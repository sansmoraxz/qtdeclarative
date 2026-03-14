pragma Singleton

import QtQml

QtObject {
    function answer(prefix: string): string {
        return prefix + "!"
    }

    function legacyAnswer(prefix) {
        return prefix + "!"
    }
}
