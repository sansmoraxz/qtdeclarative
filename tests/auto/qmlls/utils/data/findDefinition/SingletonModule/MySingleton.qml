// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

pragma Singleton

import QtQml

QtObject {
    readonly property int value: 42

    function answer(prefix: string): string {
        return prefix + value
    }

    function legacyAnswer(prefix) {
        return prefix + value
    }
}
