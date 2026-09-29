/*
 * SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts

/*
 * The "Application Menu Bar" element: the top level entries of the active
 * window's menu. The widget may contain two instances of it (the second one for
 * maximized windows), so the visible one registers itself as widget.appMenuBar,
 * and only that one shows the buttons.
 */
Item {
    id: appMenuBar

    property var modelData
    property var widget

    readonly property alias buttonGrid: menuRow
    readonly property bool registered: widget !== undefined && widget !== null && widget.appMenuBar === appMenuBar
    readonly property real naturalWidth: registered ? menuRow.implicitWidth : 0
    readonly property real progress: {
        if (!widget) {
            return 0;
        }
        if (widget.appMenuRevealOnHover) {
            return widget.appMenuProgress;
        }
        return widget.appMenuHasMenuStable ? 1 : 0;
    }
    readonly property real shownWidth: Math.round(naturalWidth * progress)
    // While the menu takes the place of a wider title, the element keeps the room of the
    // title: the widget must not shrink under the pointer, which would then leave it and
    // collapse the menu again.
    readonly property real keptWidth: widget && widget.appMenuRevealOnHover ? Math.max(0, widget.appMenuTitleWidth * progress - shownWidth) : 0
    readonly property bool hovered: hoverHandler.hovered

    function buttonAt(index) {
        return buttonRepeater.itemAt(index);
    }

    function firstButtonIndex() {
        for (let i = 0; i < buttonRepeater.count; i++) {
            const button = buttonRepeater.itemAt(i);
            if (button && button.visible && button.enabled) {
                return i;
            }
        }
        return -1;
    }

    function updateRegistration() {
        if (!widget) {
            return;
        }
        if (visible) {
            widget.appMenuBar = appMenuBar;
        } else if (widget.appMenuBar === appMenuBar) {
            widget.appMenuBar = null;
        }
    }

    implicitHeight: widget ? widget.elementHeight : 0
    Layout.alignment: widget ? widget.widgetAlignment : Qt.AlignVCenter
    Layout.fillHeight: true
    // While revealing on hover, only the minimum and maximum widths follow the
    // animation. The preferred width stays constant, so the widget does not
    // recalculate its layout on every frame.
    Layout.preferredWidth: widget && widget.appMenuRevealOnHover ? 0 : shownWidth
    Layout.minimumWidth: shownWidth + keptWidth
    Layout.maximumWidth: shownWidth + keptWidth
    clip: shownWidth < naturalWidth
    // Deliberately not tied to the menu: in the "Hide" disabled mode the element
    // would disappear, unregister and never see a menu again.
    enabled: widget ? widget.tasksModel.hasActiveWindow && !widget.vertical : false

    onVisibleChanged: updateRegistration()
    onWidgetChanged: updateRegistration()
    Component.onCompleted: updateRegistration()
    Component.onDestruction: {
        if (widget && widget.appMenuBar === appMenuBar) {
            widget.appMenuBar = null;
        }
    }

    RowLayout {
        id: menuRow

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        // Not filling the parent: while the bar is revealed, the buttons slide out
        // from under the clip instead of being squeezed.
        width: implicitWidth
        spacing: 0

        Accessible.role: Accessible.MenuBar
        Accessible.name: i18n("Application menu")

        // Only the entries count, not the room kept for the title.
        HoverHandler {
            id: hoverHandler
        }

        Repeater {
            id: buttonRepeater

            model: appMenuBar.registered && appMenuBar.widget.appMenuModel ? appMenuBar.widget.appMenuModel : null

            delegate: AppMenuButton {
                required property int index
                required property string activeMenu
                required property var activeActions
                // Read by the controller to find the button under the pointer.
                readonly property int buttonIndex: index

                Layout.fillHeight: true
                text: activeMenu
                visible: text !== "" && (activeActions?.visible ?? false)
                enabled: activeActions?.enabled ?? false
                down: appMenuBar.widget.appMenuCurrentIndex === index
                menuIsOpen: appMenuBar.widget.appMenuOpen
                mnemonicsVisible: appMenuBar.widget.appMenuAltPressed
                onActivated: appMenuBar.widget.appMenuController.trigger(this, index)
                onSwitchRequested: appMenuBar.widget.appMenuController.switchTo(index)
            }
        }
    }
}
