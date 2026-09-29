/*
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2016 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <qqmlregistration.h>

#include <memory>

class QAction;
class QDBusServiceWatcher;
class QLineEdit;
class QMenu;

namespace AtbDBusMenu
{
class DBusMenuImporter;
}

/**
 * Exposes the top level entries of an application menu that is exported over
 * the com.canonical.dbusmenu protocol.
 *
 * Adapted from AppMenuModel of the Plasma Global Menu applet. The active window
 * tracking was removed: the widget already knows the active window and passes
 * its menu address in through setMenu().
 */
class AtbAppMenuModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AppMenuModel)

    Q_PROPERTY(QString serviceName READ serviceName NOTIFY menuChanged)
    Q_PROPERTY(QString menuObjectPath READ menuObjectPath NOTIFY menuChanged)
    Q_PROPERTY(bool menuAvailable READ menuAvailable NOTIFY menuAvailableChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool searchEnabled READ searchEnabled WRITE setSearchEnabled NOTIFY searchEnabledChanged)

public:
    enum Role {
        MenuRole = Qt::UserRole + 1,
        ActionRole,
    };
    Q_ENUM(Role)

    explicit AtbAppMenuModel(QObject *parent = nullptr);
    ~AtbAppMenuModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * Switches to the menu exported at @p serviceName / @p menuObjectPath.
     * Empty values clear the menu. Passing the current pair again does nothing.
     */
    Q_INVOKABLE void setMenu(const QString &serviceName, const QString &menuObjectPath);

    QString serviceName() const;
    QString menuObjectPath() const;
    bool menuAvailable() const;
    int count() const;

    bool searchEnabled() const;
    void setSearchEnabled(bool enabled);

    /// The top level action shown at @p row, or nullptr.
    QAction *actionAt(int row) const;

    /// Asks the application to refresh @p menu, which may be a proxy menu that
    /// currently shows the actions of a top level entry. Menus that do not come from
    /// the application, like the one of the search entry, are left alone.
    void refreshMenu(QMenu *menu);

    /// Returns the row of the top level entry whose submenu contains @p action.
    int rowForAction(QAction *action) const;

Q_SIGNALS:
    void menuChanged();
    void menuAvailableChanged();
    void countChanged();
    void searchEnabledChanged();

    /// The application asked to open the top level entry at @p index (e.g. Alt+F).
    void requestActivateIndex(int index);

    /// Emitted right before the menu actions of the current application are
    /// deleted. Anything that still shows them has to let go of them now.
    void menuAboutToBeDestroyed();

private:
    void setMenuAvailable(bool available);
    void scheduleReset();
    void resetModel();
    void destroyImporter();
    void onMenuUpdated(QMenu *menu);
    void onActionChanged();
    void onActionActivationRequested(QAction *action);
    void onServiceUnregistered(const QString &serviceName);

    void createSearch();
    void destroySearch();
    QMenu *searchResultsMenu() const;
    void removeSearchResults();
    void updateSearchResults();

    QString m_serviceName;
    QString m_menuObjectPath;
    bool m_menuAvailable = false;
    bool m_resetPending = false;
    bool m_searchEnabled = false;
    int m_lastCount = 0;

    QDBusServiceWatcher *const m_serviceWatcher;
    std::unique_ptr<AtbDBusMenu::DBusMenuImporter> m_importer;
    QPointer<QMenu> m_menu;
    // Snapshot of the rows, refreshed together with the model reset so that
    // views never see rows that the model did not announce.
    QList<QPointer<QAction>> m_rows;

    std::unique_ptr<QMenu> m_searchMenu;
    QPointer<QAction> m_searchAction;
    QPointer<QLineEdit> m_searchField;
    QList<QPointer<QAction>> m_searchResults;
};
