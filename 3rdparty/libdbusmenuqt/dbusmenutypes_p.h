/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2009 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

// Qt
#include <QList>
#include <QStringList>
#include <QVariant>

class QDBusArgument;

// Application Title Bar AppMenu: the vendored types live in their own namespace so
// that their QMetaType names do not clash with other copies of libdbusmenuqt that are
// statically linked into plugins loaded by the same plasmashell process.
namespace AtbDBusMenu
{

//// DBusMenuItem
/**
 * Internal struct used to communicate on DBus
 */
struct DBusMenuItem {
    int id;
    QVariantMap properties;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItem &item);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItem &item);

typedef QList<DBusMenuItem> DBusMenuItemList;

//// DBusMenuItemKeys
/**
 * Represents a list of keys for a menu item
 */
struct DBusMenuItemKeys {
    int id;
    QStringList properties;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItemKeys &);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItemKeys &);

typedef QList<DBusMenuItemKeys> DBusMenuItemKeysList;

//// DBusMenuLayoutItem
/**
 * Represents an item with its children. GetLayout() returns a
 * DBusMenuLayoutItemList.
 */
struct DBusMenuLayoutItem;
struct DBusMenuLayoutItem {
    int id;
    QVariantMap properties;
    QList<DBusMenuLayoutItem> children;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuLayoutItem &);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuLayoutItem &);

typedef QList<DBusMenuLayoutItem> DBusMenuLayoutItemList;

//// DBusMenuShortcut

class DBusMenuShortcut;

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuShortcut &);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuShortcut &);

void DBusMenuTypes_register();

} // namespace AtbDBusMenu

// The generated D-Bus interface refers to these types without a namespace.
using namespace AtbDBusMenu;
