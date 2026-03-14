import QtQuick
import MySingletonModule
import MySingletonModule as MSM

Item {
    property string direct: MySingleton.answer("Qt")
    property string qualified: MSM.MySingleton.answer("Qt")
}
