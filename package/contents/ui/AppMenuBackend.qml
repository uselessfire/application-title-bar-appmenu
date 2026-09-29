/*
 * SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import com.github.uselessfire.applicationtitlebar.appmenu as AppMenu
import "utils.js" as Utils

/*
 * The only file that imports the compiled application menu module. main.qml loads
 * it with a Loader, so the widget keeps working when the module is not installed.
 * It exists once per widget, whatever the number of menu bar elements.
 */
Item {
    id: backend

    property var widget
    property var tasksModel
    property bool syncPending: false

    readonly property alias model: appMenuModel
    readonly property alias controller: appMenuController

    function sync() {
        if (!tasksModel || !widget) {
            return;
        }
        // Keep the menu of the application whose menu is open, and do not follow the
        // focus to the panel when it is activated with the keyboard.
        if (widget.appMenuOpen || Plasmoid.containment.status === PlasmaCore.Types.AcceptingInputStatus) {
            syncPending = true;
            return;
        }
        syncPending = false;
        const window = tasksModel.hasActiveWindow ? tasksModel.activeWindow : null;
        appMenuModel.setMenu(window ? window.appMenuServiceName : "", window ? window.appMenuObjectPath : "");
    }

    onTasksModelChanged: sync()
    onWidgetChanged: sync()

    AppMenu.AppMenuModel {
        id: appMenuModel

        // Like the stock Global Menu, the search entry is only offered on Wayland.
        searchEnabled: Utils.isWayland() && plasmoid.configuration.appMenuSearchEnabled
        onRequestActivateIndex: index => backend.widget.appMenuActivate(index)
        // A menu requested before is not the one of another window.
        onMenuChanged: {
            if (backend.widget) {
                backend.widget.appMenuPendingIndex = -1;
            }
        }
    }

    AppMenu.AppMenuController {
        id: appMenuController

        model: appMenuModel
        buttonGrid: backend.widget && backend.widget.appMenuBar ? backend.widget.appMenuBar.buttonGrid : null
        popupEdge: {
            switch (Plasmoid.location) {
            case PlasmaCore.Types.TopEdge:
                return Qt.TopEdge;
            case PlasmaCore.Types.BottomEdge:
                return Qt.BottomEdge;
            case PlasmaCore.Types.LeftEdge:
                return Qt.LeftEdge;
            case PlasmaCore.Types.RightEdge:
                return Qt.RightEdge;
            default:
                return 0;
            }
        }
    }

    Connections {
        // null until main.qml sets it: an undefined target falls back to the parent.
        target: backend.tasksModel ?? null
        function onActiveWindowUpdated() {
            backend.sync();
        }
    }

    Connections {
        target: backend.widget ?? null
        function onAppMenuOpenChanged() {
            if (backend.syncPending) {
                backend.sync();
            }
        }
    }

    Connections {
        target: Plasmoid.containment
        function onStatusChanged() {
            if (backend.syncPending) {
                backend.sync();
            }
        }
    }
}
