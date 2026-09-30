/*
 * SPDX-FileCopyrightText: 2020 Carson Black <uhhadd@gmail.com>
 * SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import org.kde.plasma.components as PlasmaComponents

// A top level entry of the menu bar, adapted from MenuDelegate of the Plasma Global Menu.
// Deliberately not a button control: a button grabs the pointer when pressed, and then the
// press cannot drag the window any more (see WidgetDragHandler).
Item {
    id: controlRoot

    enum State {
        Rest,
        Hover,
        Down
    }

    property string text
    // The menu of this entry is open.
    property bool down: false
    property bool menuIsOpen: false
    property bool mnemonicsVisible: false
    readonly property bool hovered: hoverHandler.hovered
    readonly property bool pressed: tapHandler.pressed
    property int menuState: {
        // The hover state cannot be trusted while the menu grabs the pointer.
        if (down || pressed) {
            return AppMenuButton.State.Down;
        } else if (hovered && !menuIsOpen) {
            return AppMenuButton.State.Hover;
        }
        return AppMenuButton.State.Rest;
    }

    // Clicked, or activated from the keyboard: opens the menu, or triggers an entry without one.
    signal activated
    // The pointer came over this entry while a menu is open: shows its menu, never triggers it.
    signal switchRequested

    implicitWidth: label.implicitWidth + frame.margins.left + frame.margins.right
    implicitHeight: label.implicitHeight + frame.margins.top + frame.margins.bottom

    Kirigami.MnemonicData.controlType: Kirigami.MnemonicData.SecondaryControl
    Kirigami.MnemonicData.label: text
    Kirigami.MnemonicData.active: mnemonicsVisible

    Accessible.role: Accessible.Button
    Accessible.name: Kirigami.MnemonicData.plainTextLabel
    Accessible.description: i18nc("@info:usagetip", "Open a menu")
    Accessible.onPressAction: activated()

    HoverHandler {
        id: hoverHandler

        // The controller does the same from the events it sees on the menu, which is the
        // reliable path while the menu grabs the pointer.
        onHoveredChanged: {
            if (hovered && controlRoot.menuIsOpen) {
                controlRoot.switchRequested();
            }
        }
    }

    TapHandler {
        id: tapHandler

        // Menus open on click, unlike in QMenuBar: a press that moves further than the drag
        // threshold drags the window like the title does, and opens nothing. The menus close
        // on any click outside of them.
        gesturePolicy: TapHandler.DragThreshold
        // A long press opens the menu as well, like with a button: no long press gesture,
        // after which a release would not count as a tap.
        longPressThreshold: 0
        acceptedButtons: Qt.LeftButton
        onTapped: controlRoot.activated()
    }

    KSvg.FrameSvgItem {
        id: frame

        anchors.fill: parent
        imagePath: "widgets/menubaritem"
        prefix: {
            switch (controlRoot.menuState) {
            case AppMenuButton.State.Down:
                return "pressed";
            case AppMenuButton.State.Hover:
                return "hover";
            default:
                return "normal";
            }
        }
    }

    PlasmaComponents.Label {
        id: label

        anchors.fill: parent
        anchors.leftMargin: frame.margins.left
        anchors.topMargin: frame.margins.top
        anchors.rightMargin: frame.margins.right
        anchors.bottomMargin: frame.margins.bottom
        text: controlRoot.Kirigami.MnemonicData.richTextLabel
        textFormat: Text.StyledText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: controlRoot.menuState === AppMenuButton.State.Rest ? Kirigami.Theme.textColor : Kirigami.Theme.highlightedTextColor
    }
}
