/*
 * SPDX-FileCopyrightText: 2020 Carson Black <uhhadd@gmail.com>
 * SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import org.kde.plasma.components as PlasmaComponents

// A top level entry of the menu bar, adapted from MenuDelegate of the Plasma Global Menu.
AbstractButton {
    id: controlRoot

    enum State {
        Rest,
        Hover,
        Down
    }

    property bool menuIsOpen: false
    property bool mnemonicsVisible: false
    property int menuState: {
        // The hover state cannot be trusted while the menu grabs the pointer.
        if (down) {
            return AppMenuButton.State.Down;
        } else if (hovered && !menuIsOpen) {
            return AppMenuButton.State.Hover;
        }
        return AppMenuButton.State.Rest;
    }

    // Pressed, or activated from the keyboard: opens the menu, or triggers an entry without one.
    signal activated
    // The pointer came over this entry while a menu is open: shows its menu, never triggers it.
    signal switchRequested

    hoverEnabled: true
    // The controller does the same from the events it sees on the menu, which is the
    // reliable path while the menu grabs the pointer.
    onHoveredChanged: {
        if (hovered && menuIsOpen) {
            switchRequested();
        }
    }
    // Menus open on press, like in QMenuBar. They close on any click outside of them.
    onPressed: activated()

    Kirigami.MnemonicData.controlType: Kirigami.MnemonicData.SecondaryControl
    Kirigami.MnemonicData.label: text
    Kirigami.MnemonicData.active: mnemonicsVisible

    topPadding: frame.margins.top
    leftPadding: frame.margins.left
    rightPadding: frame.margins.right
    bottomPadding: frame.margins.bottom

    Accessible.name: Kirigami.MnemonicData.plainTextLabel
    Accessible.description: i18nc("@info:usagetip", "Open a menu")
    Accessible.onPressAction: activated()

    background: KSvg.FrameSvgItem {
        id: frame

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

    contentItem: PlasmaComponents.Label {
        text: controlRoot.Kirigami.MnemonicData.richTextLabel
        textFormat: Text.StyledText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: controlRoot.menuState === AppMenuButton.State.Rest ? Kirigami.Theme.textColor : Kirigami.Theme.highlightedTextColor
    }
}
