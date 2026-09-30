/*
 * SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import "../package/contents/ui" as Widget

// The menu button of the widget above a PointHandler, like the buttons of the menu bar
// above the window drag handler of the widget (WidgetDragHandler in MouseHandlers.qml).
Window {
    id: scene

    property int activations: 0
    property int switches: 0
    property int dragActivations: 0
    property real dragDistance: 0
    property alias button: button

    width: 300
    height: 40
    visible: true

    Item {
        anchors.fill: parent

        PointHandler {
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active) {
                    scene.dragActivations++;
                }
            }
            onPointChanged: {
                if (active) {
                    scene.dragDistance = Math.max(scene.dragDistance, point.position.x - point.pressPosition.x);
                }
            }
        }
    }

    Widget.AppMenuButton {
        id: button

        width: 100
        height: 40
        text: "&File"
        onActivated: scene.activations++
        onSwitchRequested: scene.switches++
    }
}
