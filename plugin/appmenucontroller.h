/*
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <qqmlregistration.h>

#include <memory>

#include "appmenumodel.h"

class QAction;
class QMenu;
class QQuickItem;

/**
 * Pops up the submenus of an AppMenuModel below the buttons of a menu bar.
 *
 * Adapted from AppMenuApplet of the Plasma Global Menu applet, without the
 * dependency on Plasma::Applet. Like the original it shows every submenu
 * through a single proxy QMenu, so moving the pointer over another button
 * switches the visible submenu without closing the popup.
 */
class AtbAppMenuController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AppMenuController)

    Q_PROPERTY(AtbAppMenuModel *model READ model WRITE setModel NOTIFY modelChanged)
    /// Item whose direct children are the menu buttons. Each button must have
    /// an integer "buttonIndex" property with its row in the model.
    Q_PROPERTY(QQuickItem *buttonGrid READ buttonGrid WRITE setButtonGrid NOTIFY buttonGridChanged)
    /// Panel edge the widget is attached to, as a Qt::Edge value, 0 if none.
    Q_PROPERTY(int popupEdge READ popupEdge WRITE setPopupEdge NOTIFY popupEdgeChanged)
    /// Row whose submenu is currently shown, -1 if no menu is open.
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

public:
    explicit AtbAppMenuController(QObject *parent = nullptr);
    ~AtbAppMenuController() override;

    AtbAppMenuModel *model() const;
    void setModel(AtbAppMenuModel *model);

    QQuickItem *buttonGrid() const;
    void setButtonGrid(QQuickItem *buttonGrid);

    int popupEdge() const;
    void setPopupEdge(int edge);

    int currentIndex() const;

    /**
     * Opens the submenu of the entry at @p index below @p button. A top level
     * entry without a submenu is triggered instead.
     */
    Q_INVOKABLE void trigger(QQuickItem *button, int index);

    /**
     * While a menu is open, shows the submenu of the entry at @p index instead.
     * Entries without a submenu or with an empty one are passed over, never triggered.
     */
    Q_INVOKABLE void switchTo(int index);

    /// Closes the open submenu, if any.
    Q_INVOKABLE void closeMenu();

Q_SIGNALS:
    void modelChanged();
    void buttonGridChanged();
    void popupEdgeChanged();
    void currentIndexChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Activation {
        Explicit, // pressed, or requested by the application
        Switch, // moving between menus with the pointer or the arrow keys
    };

    void openMenu(QQuickItem *button, int index, Activation activation);
    void watchSubmenus(QMenu *menu);
    int neighbourIndex(int step) const;
    QQuickItem *buttonForIndex(int index) const;
    QPoint popupPosition(QQuickItem *button) const;
    void ensureProxyMenu();
    void restoreSourceMenu();
    void scheduleReposition();
    void updatePanelHover();
    void onMenuAboutToHide();
    void setCurrentIndex(int index);

    QPointer<AtbAppMenuModel> m_model;
    QPointer<QQuickItem> m_buttonGrid;
    int m_popupEdge = 0;
    int m_currentIndex = -1;
    // How the style moved the menu when it was shown last
    QPoint m_styleOffset;
    // Whether the compositor closed the menu, e.g. for a click elsewhere
    bool m_closedByCompositor = false;
    bool m_repositionPending = false;

    // The proxy shows the actions of m_sourceMenu while it is open. The top level
    // action points to the proxy meanwhile, so the importer keeps updating it.
    std::unique_ptr<QMenu> m_proxyMenu;
    QPointer<QAction> m_sourceAction;
    QPointer<QMenu> m_sourceMenu;
};
