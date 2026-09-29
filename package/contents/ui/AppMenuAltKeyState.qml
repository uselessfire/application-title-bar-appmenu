/*
 * SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import org.kde.plasma.private.keyboardindicator as KeyboardIndicator

// Whether Alt is held, to underline the mnemonics like the stock Global Menu does.
// Loaded with a Loader: the module is private to Plasma and may go away.
KeyboardIndicator.KeyState {
    key: Qt.Key_Alt
}
