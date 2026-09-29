/*
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2016 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "appmenumodel.h"
#include "appmenudebug.h"

#include <dbusmenuimporter.h>

#include <KLocalizedString>

#include <QAction>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QLineEdit>
#include <QMenu>
#include <QWidgetAction>

using AtbDBusMenu::DBusMenuImporter;

namespace
{
constexpr auto SearchObjectName = "appmenu-search";
constexpr auto DBusMenuIdProperty = "_dbusmenu_id";
// The search strings are the ones of the stock Global Menu applet, so its
// translations, which are installed with plasma-workspace, apply here too.
constexpr auto StockAppletDomain = "plasma_applet_org.kde.plasma.appmenu";
}

AtbAppMenuModel::AtbAppMenuModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
{
    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, &AtbAppMenuModel::onServiceUnregistered);
}

AtbAppMenuModel::~AtbAppMenuModel()
{
    destroySearch();
    destroyImporter();
}

int AtbAppMenuModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int AtbAppMenuModel::count() const
{
    return rowCount();
}

QVariant AtbAppMenuModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    QAction *action = m_rows.at(index.row());
    if (!action) {
        return {};
    }

    switch (role) {
    case MenuRole:
        return action->text();
    case ActionRole:
        return QVariant::fromValue(action);
    }
    return {};
}

QHash<int, QByteArray> AtbAppMenuModel::roleNames() const
{
    return {
        {MenuRole, QByteArrayLiteral("activeMenu")},
        {ActionRole, QByteArrayLiteral("activeActions")},
    };
}

QString AtbAppMenuModel::serviceName() const
{
    return m_serviceName;
}

QString AtbAppMenuModel::menuObjectPath() const
{
    return m_menuObjectPath;
}

bool AtbAppMenuModel::menuAvailable() const
{
    return m_menuAvailable;
}

void AtbAppMenuModel::setMenuAvailable(bool available)
{
    if (m_menuAvailable != available) {
        m_menuAvailable = available;
        Q_EMIT menuAvailableChanged();
    }
}

bool AtbAppMenuModel::searchEnabled() const
{
    return m_searchEnabled;
}

void AtbAppMenuModel::setSearchEnabled(bool enabled)
{
    if (m_searchEnabled == enabled) {
        return;
    }
    m_searchEnabled = enabled;
    if (enabled) {
        createSearch();
    } else {
        destroySearch();
    }
    scheduleReset();
    Q_EMIT searchEnabledChanged();
}

QAction *AtbAppMenuModel::actionAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return nullptr;
    }
    return m_rows.at(row);
}

int AtbAppMenuModel::rowForAction(QAction *action) const
{
    if (!action) {
        return -1;
    }
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row) == action) {
            return row;
        }
    }
    // A nested action: find the top level submenu it belongs to. Submenus are
    // created as children of their parent menu, so walk up the QObject tree.
    QMenu *topLevelMenu = nullptr;
    for (QObject *object = action->parent(); object && object != m_menu; object = object->parent()) {
        if (auto *menu = qobject_cast<QMenu *>(object)) {
            topLevelMenu = menu;
        }
    }
    if (!topLevelMenu) {
        return -1;
    }
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row) && m_rows.at(row)->menu() == topLevelMenu) {
            return row;
        }
    }
    return -1;
}

void AtbAppMenuModel::refreshMenu(QMenu *menu)
{
    // Only the menus of the application have an id, not the one of the search entry.
    if (m_importer && menu && menu->menuAction()->property(DBusMenuIdProperty).isValid()) {
        m_importer->updateMenu(menu);
    }
}

void AtbAppMenuModel::setMenu(const QString &serviceName, const QString &menuObjectPath)
{
    // The widget calls this whenever anything about the active window changes,
    // e.g. its title, so an unchanged address must not cause any D-Bus traffic.
    if (m_serviceName == serviceName && m_menuObjectPath == menuObjectPath) {
        return;
    }

    qCDebug(ATB_APPMENU) << "Menu address changed to" << serviceName << menuObjectPath;
    destroyImporter();
    m_serviceName = serviceName;
    m_menuObjectPath = menuObjectPath;

    if (serviceName.isEmpty() || menuObjectPath.isEmpty()) {
        m_serviceWatcher->setWatchedServices({});
    } else {
        m_serviceWatcher->setWatchedServices({serviceName});
        m_importer = std::make_unique<DBusMenuImporter>(serviceName, menuObjectPath);
        connect(m_importer.get(), &DBusMenuImporter::menuUpdated, this, &AtbAppMenuModel::onMenuUpdated);
        connect(m_importer.get(), &DBusMenuImporter::actionActivationRequested, this, &AtbAppMenuModel::onActionActivationRequested);
        // Ask the application to prepare the root menu (AboutToShow), like QMenuBar would.
        QMetaObject::invokeMethod(
            m_importer.get(),
            [importer = m_importer.get()] {
                importer->updateMenu();
            },
            Qt::QueuedConnection);
    }

    Q_EMIT menuChanged();
    scheduleReset();
}

void AtbAppMenuModel::destroyImporter()
{
    if (!m_importer && !m_menu && m_rows.isEmpty()) {
        return;
    }
    Q_EMIT menuAboutToBeDestroyed();
    removeSearchResults();
    if (m_searchField) {
        m_searchField->clear();
    }
    m_menu = nullptr;
    m_importer.reset();
    setMenuAvailable(false);
    scheduleReset();
}

void AtbAppMenuModel::onServiceUnregistered(const QString &serviceName)
{
    if (serviceName != m_serviceName) {
        return;
    }
    qCDebug(ATB_APPMENU) << "Menu service" << serviceName << "is gone";
    // The application is gone: drop its menu instead of keeping stale actions around.
    // Forget the address as well, so a later setMenu() with it creates a new importer.
    destroyImporter();
    m_serviceName.clear();
    m_menuObjectPath.clear();
    m_serviceWatcher->setWatchedServices({});
    Q_EMIT menuChanged();
}

void AtbAppMenuModel::onMenuUpdated(QMenu *menu)
{
    if (!m_importer || menu != m_importer->menu()) {
        // Updates of submenus do not change the top level entries.
        return;
    }

    m_menu = menu;
    const auto actions = m_menu->actions();
    if (actions.isEmpty()) {
        // Some applications pretend to have a menu but do not export any entries.
        setMenuAvailable(false);
        scheduleReset();
        return;
    }

    for (QAction *action : actions) {
        connect(action, &QAction::changed, this, &AtbAppMenuModel::onActionChanged, Qt::UniqueConnection);
        connect(action, &QObject::destroyed, this, &AtbAppMenuModel::scheduleReset, Qt::UniqueConnection);
        // Cache the first level of submenus, which are the ones we pop up.
        if (QMenu *submenu = action->menu()) {
            m_importer->updateMenu(submenu);
        }
    }

    setMenuAvailable(true);
    scheduleReset();
}

void AtbAppMenuModel::onActionChanged()
{
    auto *action = qobject_cast<QAction *>(sender());
    const int row = int(m_rows.indexOf(action));
    if (row >= 0) {
        Q_EMIT dataChanged(index(row), index(row));
    }
}

void AtbAppMenuModel::onActionActivationRequested(QAction *action)
{
    const int row = rowForAction(action);
    if (row >= 0) {
        Q_EMIT requestActivateIndex(row);
    }
}

void AtbAppMenuModel::scheduleReset()
{
    if (m_resetPending) {
        return;
    }
    m_resetPending = true;
    QMetaObject::invokeMethod(this, &AtbAppMenuModel::resetModel, Qt::QueuedConnection);
}

void AtbAppMenuModel::resetModel()
{
    m_resetPending = false;

    beginResetModel();
    m_rows.clear();
    if (m_menuAvailable && m_menu) {
        const auto actions = m_menu->actions();
        for (QAction *action : actions) {
            m_rows.append(action);
        }
        if (m_searchAction) {
            m_rows.append(m_searchAction.data());
        }
    }
    endResetModel();
    qCDebug(ATB_APPMENU) << "Model reset with" << m_rows.size() << "entries";

    if (m_searchField) {
        updateSearchResults();
    }

    if (m_lastCount != m_rows.size()) {
        m_lastCount = int(m_rows.size());
        Q_EMIT countChanged();
    }
}

// Search entry, only offered on Wayland (like the Global Menu applet, see the QML side).

void AtbAppMenuModel::createSearch()
{
    if (m_searchAction) {
        return;
    }

    m_searchMenu = std::make_unique<QMenu>();

    auto *searchWidgetAction = new QWidgetAction(m_searchMenu.get());
    auto *searchField = new QLineEdit;
    searchField->setClearButtonEnabled(true);
    searchField->setPlaceholderText(i18nd(StockAppletDomain, "Search…"));
    searchField->setMinimumWidth(200);
    searchField->setContentsMargins(4, 4, 4, 4);
    connect(searchField, &QLineEdit::textChanged, this, &AtbAppMenuModel::updateSearchResults);
    connect(searchField, &QLineEdit::returnPressed, this, [this] {
        for (const auto &result : std::as_const(m_searchResults)) {
            if (result) {
                result->trigger();
                return;
            }
        }
    });
    searchWidgetAction->setDefaultWidget(searchField);
    m_searchField = searchField;

    m_searchMenu->addAction(searchWidgetAction);
    m_searchMenu->addSeparator();

    m_searchAction = new QAction(this);
    m_searchAction->setText(i18nd(StockAppletDomain, "Search"));
    m_searchAction->setObjectName(QString::fromLatin1(SearchObjectName));
    m_searchAction->setMenu(m_searchMenu.get());
}

void AtbAppMenuModel::destroySearch()
{
    removeSearchResults();
    delete m_searchAction.data();
    m_searchMenu.reset();
}

QMenu *AtbAppMenuModel::searchResultsMenu() const
{
    // While the search entry is open, its entries are in the menu that shows it.
    return m_searchAction ? m_searchAction->menu() : nullptr;
}

void AtbAppMenuModel::removeSearchResults()
{
    if (QMenu *menu = searchResultsMenu()) {
        for (const auto &result : std::as_const(m_searchResults)) {
            if (result) {
                menu->removeAction(result);
            }
        }
    }
    m_searchResults.clear();
}

void AtbAppMenuModel::updateSearchResults()
{
    removeSearchResults();
    QMenu *resultsMenu = searchResultsMenu();
    if (!resultsMenu || !m_searchField || !m_menu || !m_menuAvailable) {
        return;
    }
    const QString filter = m_searchField->text();
    if (filter.isEmpty()) {
        return;
    }
    const auto actions = m_menu->findChildren<QAction *>();
    for (QAction *action : actions) {
        if (action->menu() || action->isSeparator() || !action->isVisible()) {
            continue;
        }
        // "Save &All" must match "save all", and "S&ave" must match "sav".
        if (KLocalizedString::removeAcceleratorMarker(action->text()).contains(filter, Qt::CaseInsensitive)) {
            resultsMenu->addAction(action);
            m_searchResults.append(action);
        }
    }
}

#include "moc_appmenumodel.cpp"
