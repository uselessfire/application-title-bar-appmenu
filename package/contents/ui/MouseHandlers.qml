/*
 * SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick

Item {
    id: handlers
    anchors.fill: parent

    // Whether a press may start dragging the window
    property bool dragEnabled: true
    // Whether clicks and the wheel act on the window
    property bool clicksEnabled: true

    signal invokeKWinShortcut(string shortcut)

    WidgetDragHandler {
        id: dragHandler
        allowed: handlers.dragEnabled
        Component.onCompleted: {
            invokeKWinShortcut.connect(handlers.invokeKWinShortcut);
        }
        onInvokeKWinShortcut: tapHandler.stopLongPressTimer()
    }

    WidgetTapHandler {
        id: tapHandler
        allowed: handlers.clicksEnabled
        Component.onCompleted: {
            invokeKWinShortcut.connect(handlers.invokeKWinShortcut);
        }
        onInvokeKWinShortcut: dragHandler.stopDrag()
    }

    WidgetWheelHandler {
        orientation: Qt.Vertical
        allowed: handlers.clicksEnabled
        Component.onCompleted: {
            invokeKWinShortcut.connect(handlers.invokeKWinShortcut);
        }
    }

    WidgetWheelHandler {
        orientation: Qt.Horizontal
        allowed: handlers.clicksEnabled
        Component.onCompleted: {
            invokeKWinShortcut.connect(handlers.invokeKWinShortcut);
        }
    }
}
