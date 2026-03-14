// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import findDefinition.SingletonModule
import findDefinition.SingletonModule as FSM

Item {
    property int singletonValue: MySingleton.value
    property int qualifiedSingletonValue: FSM.MySingleton.value
    property string singletonText: MySingleton.answer("Qt")
    property string qualifiedSingletonText: FSM.MySingleton.answer("Qt")
    property string singletonLegacyText: MySingleton.legacyAnswer("Qt")
    property string qualifiedSingletonLegacyText: FSM.MySingleton.legacyAnswer("Qt")
}
