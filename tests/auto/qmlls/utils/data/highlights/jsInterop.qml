import QtQuick
import "./Helper.js" as Helper

Item {
    function test(value) {
        const fromJs = Helper.answer(value)
        const asJson = JSON.stringify(value)
        return [fromJs, asJson]
    }
}
