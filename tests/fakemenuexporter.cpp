/*
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "fakemenuexporter.h"

FakeMenuExporter::FakeMenuExporter(const QString &connectionName, const QString &path)
    : m_connectionName(connectionName)
    , m_path(path)
    , m_connection(QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName))
{
    DBusMenuTypes_register();
    auto *adaptor = new FakeMenuAdaptor(this);
    connect(this, &FakeMenuExporter::layoutUpdated, adaptor, &FakeMenuAdaptor::LayoutUpdated);
    connect(this, &FakeMenuExporter::itemActivationRequested, adaptor, &FakeMenuAdaptor::ItemActivationRequested);
    connect(this, &FakeMenuExporter::itemsPropertiesUpdated, adaptor, &FakeMenuAdaptor::ItemsPropertiesUpdated);
    m_connection.registerObject(m_path, this);
}

FakeMenuExporter::~FakeMenuExporter()
{
    disconnectFromBus();
}

QString FakeMenuExporter::service() const
{
    return m_connection.baseService();
}

QString FakeMenuExporter::path() const
{
    return m_path;
}

FakeMenuExporter::Item &FakeMenuExporter::root()
{
    return m_root;
}

void FakeMenuExporter::setRoot(const Item &root)
{
    m_root = root;
}

void FakeMenuExporter::emitLayoutUpdated(int parentId)
{
    ++revision;
    Q_EMIT layoutUpdated(revision, parentId);
}

void FakeMenuExporter::emitItemActivationRequested(int id)
{
    Q_EMIT itemActivationRequested(id, 0);
}

void FakeMenuExporter::emitItemsPropertiesUpdated(const QList<AtbDBusMenu::DBusMenuItem> &updated)
{
    Q_EMIT itemsPropertiesUpdated(updated, {});
}

void FakeMenuExporter::disconnectFromBus()
{
    if (m_connection.isConnected()) {
        m_connection.unregisterObject(m_path);
        // The connection is only closed once no QDBusConnection refers to it anymore.
        m_connection = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(m_connectionName);
    }
}

const FakeMenuExporter::Item *FakeMenuExporter::find(const Item &item, int id) const
{
    if (item.id == id) {
        return &item;
    }
    for (const Item &child : item.children) {
        if (const Item *found = find(child, id)) {
            return found;
        }
    }
    return nullptr;
}

DBusMenuLayoutItem FakeMenuExporter::toLayout(const Item &item, int recursionDepth) const
{
    DBusMenuLayoutItem layout;
    layout.id = item.id;
    if (item.id != 0) {
        layout.properties.insert(QStringLiteral("label"), item.label);
        layout.properties.insert(QStringLiteral("visible"), item.visible);
        layout.properties.insert(QStringLiteral("enabled"), item.enabled);
    }
    if (item.submenu || item.id == 0) {
        layout.properties.insert(QStringLiteral("children-display"), QStringLiteral("submenu"));
    }
    if (recursionDepth != 0) {
        for (const Item &child : item.children) {
            layout.children.append(toLayout(child, recursionDepth < 0 ? -1 : recursionDepth - 1));
        }
    }
    return layout;
}

DBusMenuLayoutItem FakeMenuExporter::layout(int parentId, int recursionDepth) const
{
    const Item *item = find(m_root, parentId);
    return item ? toLayout(*item, recursionDepth) : DBusMenuLayoutItem{parentId, {}, {}};
}

FakeMenuAdaptor::FakeMenuAdaptor(FakeMenuExporter *exporter)
    : QDBusAbstractAdaptor(exporter)
    , m_exporter(exporter)
{
}

uint FakeMenuAdaptor::version() const
{
    return 3;
}

QString FakeMenuAdaptor::status() const
{
    return QStringLiteral("normal");
}

uint FakeMenuAdaptor::GetLayout(int parentId, int recursionDepth, const QStringList &propertyNames, AtbDBusMenu::DBusMenuLayoutItem &layout)
{
    Q_UNUSED(propertyNames)
    layout = m_exporter->layout(parentId, recursionDepth);
    return m_exporter->revision;
}

QList<AtbDBusMenu::DBusMenuItem> FakeMenuAdaptor::GetGroupProperties(const QList<int> &ids, const QStringList &propertyNames)
{
    Q_UNUSED(propertyNames)
    QList<AtbDBusMenu::DBusMenuItem> items;
    for (int id : ids) {
        const DBusMenuLayoutItem layout = m_exporter->layout(id, 0);
        items.append(DBusMenuItem{layout.id, layout.properties});
    }
    return items;
}

QDBusVariant FakeMenuAdaptor::GetProperty(int id, const QString &name)
{
    return QDBusVariant(m_exporter->layout(id, 0).properties.value(name));
}

bool FakeMenuAdaptor::AboutToShow(int id)
{
    m_exporter->aboutToShowCalls.append(id);
    return false;
}

void FakeMenuAdaptor::Event(int id, const QString &eventId, const QDBusVariant &data, uint timestamp)
{
    Q_UNUSED(data)
    Q_UNUSED(timestamp)
    m_exporter->events.append({id, eventId});
}

#include "moc_fakemenuexporter.cpp"
