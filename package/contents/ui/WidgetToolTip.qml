/*
 * SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import org.kde.plasma.core as PlasmaCore
import org.kde.taskmanager as TaskManager

PlasmaCore.ToolTipArea {
    enum ToolTipMode {
        Disabled,
        MaximizedWindow,
        Always
    }

    active: showToolTip() && !suppressed
    mainText: tasksModel.activeWindow.genericAppName || ""
    icon: tasksModel.activeWindow.icon

    property TaskManager.TasksModel tasksModel
    // Set while the application menu is in use, the tooltip would cover it.
    property bool suppressed: false

    function showToolTip() {
        switch (plasmoid.configuration.widgetToolTipMode) {
        case WidgetToolTip.ToolTipMode.Disabled:
            return false;
        case WidgetToolTip.ToolTipMode.MaximizedWindow:
            return tasksModel.hasActiveWindow && tasksModel.activeWindow.maximized;
        default:
            return tasksModel.hasActiveWindow;
        }
    }
}
