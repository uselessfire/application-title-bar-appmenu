/*
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusVariant>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

#include <dbusmenutypes_p.h>

/**
 * A minimal com.canonical.dbusmenu exporter, standing in for an application
 * that exports its menu bar.
 */
class FakeMenuExporter : public QObject
{
    Q_OBJECT

public:
    struct Item {
        int id = 0;
        QString label;
        bool submenu = false;
        bool visible = true;
        bool enabled = true;
        QList<Item> children;
    };

    /// Registers the menu at @p path on a private connection named @p connectionName.
    FakeMenuExporter(const QString &connectionName, const QString &path);
    ~FakeMenuExporter() override;

    QString service() const;
    QString path() const;

    Item &root();
    void setRoot(const Item &root);

    /// Tells the importers that the children of @p parentId changed.
    void emitLayoutUpdated(int parentId = 0);
    void emitItemActivationRequested(int id);
    void emitItemsPropertiesUpdated(const QList<AtbDBusMenu::DBusMenuItem> &updated);

    /// Drops the connection, as if the application quit.
    void disconnectFromBus();

    DBusMenuLayoutItem layout(int parentId, int recursionDepth) const;

    QList<int> aboutToShowCalls;
    QList<QPair<int, QString>> events;
    uint revision = 1;

Q_SIGNALS:
    void layoutUpdated(uint revision, int parent);
    void itemActivationRequested(int id, uint timestamp);
    void itemsPropertiesUpdated(const QList<AtbDBusMenu::DBusMenuItem> &updatedProps, const QList<AtbDBusMenu::DBusMenuItemKeys> &removedProps);

private:
    const Item *find(const Item &item, int id) const;
    DBusMenuLayoutItem toLayout(const Item &item, int recursionDepth) const;

    QString m_connectionName;
    QString m_path;
    QDBusConnection m_connection;
    Item m_root;
};

class FakeMenuAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")
    Q_PROPERTY(uint Version READ version)
    Q_PROPERTY(QString Status READ status)

public:
    explicit FakeMenuAdaptor(FakeMenuExporter *exporter);

    uint version() const;
    QString status() const;

    // QtDBus resolves some parameter types by name, hence the fully qualified types.
public Q_SLOTS:
    uint GetLayout(int parentId, int recursionDepth, const QStringList &propertyNames, AtbDBusMenu::DBusMenuLayoutItem &layout);
    QList<AtbDBusMenu::DBusMenuItem> GetGroupProperties(const QList<int> &ids, const QStringList &propertyNames);
    QDBusVariant GetProperty(int id, const QString &name);
    bool AboutToShow(int id);
    Q_NOREPLY void Event(int id, const QString &eventId, const QDBusVariant &data, uint timestamp);

Q_SIGNALS:
    void LayoutUpdated(uint revision, int parent);
    void ItemsPropertiesUpdated(const QList<AtbDBusMenu::DBusMenuItem> &updatedProps, const QList<AtbDBusMenu::DBusMenuItemKeys> &removedProps);
    void ItemActivationRequested(int id, uint timestamp);

private:
    FakeMenuExporter *const m_exporter;
};
