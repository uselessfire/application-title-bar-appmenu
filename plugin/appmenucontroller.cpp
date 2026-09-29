/*
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "appmenucontroller.h"
#include "appmenudebug.h"

#include <QAction>
#include <QActionEvent>
#include <QCursor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

namespace
{
// While somebody owns org.kde.kappmenuview, the kded appmenu module provides the
// com.canonical.AppMenu.Registrar service, and applications export their menus
// instead of showing their own menu bar.
//
// A D-Bus name does not count references, and other Global Menu widgets in the same
// plasmashell register it on the shared session bus connection. The name is therefore
// owned through a private connection: releasing it there never takes it away from them.
constexpr auto ViewService = "org.kde.kappmenuview";
constexpr auto ViewConnectionName = "com.github.uselessfire.applicationtitlebar.appmenu.view";
int s_viewServiceUsers = 0;
bool s_viewServiceRequested = false;

void acquireViewService()
{
    ++s_viewServiceUsers;
    // Until it works, every new user tries again.
    if (s_viewServiceRequested) {
        return;
    }
    const QString connectionName = QString::fromLatin1(ViewConnectionName);
    const QDBusConnection connection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
    if (!connection.isConnected() || !connection.interface()) {
        qWarning("Application Title Bar: cannot connect to the session bus to register %s", ViewService);
        QDBusConnection::disconnectFromBus(connectionName);
        return;
    }
    const auto reply = connection.interface()->registerService(QString::fromLatin1(ViewService),
                                                               QDBusConnectionInterface::QueueService,
                                                               QDBusConnectionInterface::DontAllowReplacement);
    if (!reply.isValid()) {
        qWarning() << "Application Title Bar: cannot register" << ViewService << reply.error().message();
        QDBusConnection::disconnectFromBus(connectionName);
        return;
    }
    s_viewServiceRequested = true;
}

void releaseViewService()
{
    if (--s_viewServiceUsers > 0 || !s_viewServiceRequested) {
        return;
    }
    // Closing the private connection releases the name (or our place in its queue).
    QDBusConnection::disconnectFromBus(QString::fromLatin1(ViewConnectionName));
    s_viewServiceRequested = false;
}

constexpr auto DBusMenuIdProperty = "_dbusmenu_id";
// The largest shift of a menu that is taken for the shadow of the style, in pixels.
constexpr int MaxStyleOffset = 64;
}

AtbAppMenuController::AtbAppMenuController(QObject *parent)
    : QObject(parent)
{
    acquireViewService();
}

AtbAppMenuController::~AtbAppMenuController()
{
    closeMenu();
    m_proxyMenu.reset();
    releaseViewService();
}

AtbAppMenuModel *AtbAppMenuController::model() const
{
    return m_model;
}

void AtbAppMenuController::setModel(AtbAppMenuModel *model)
{
    if (m_model == model) {
        return;
    }
    closeMenu();
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    m_model = model;
    if (m_model) {
        // The actions shown in the proxy belong to the model's importer.
        connect(m_model, &AtbAppMenuModel::menuAboutToBeDestroyed, this, &AtbAppMenuController::closeMenu);
        connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
            if (m_currentIndex < 0) {
                return;
            }
            // The rows were rebuilt: follow the open entry or close the menu if it is gone.
            const int row = m_sourceAction ? m_model->rowForAction(m_sourceAction) : -1;
            if (row < 0) {
                closeMenu();
            } else {
                setCurrentIndex(row);
                // Its button may have moved.
                scheduleReposition();
            }
        });
    }
    Q_EMIT modelChanged();
}

QQuickItem *AtbAppMenuController::buttonGrid() const
{
    return m_buttonGrid;
}

void AtbAppMenuController::setButtonGrid(QQuickItem *buttonGrid)
{
    if (m_buttonGrid != buttonGrid) {
        m_buttonGrid = buttonGrid;
        Q_EMIT buttonGridChanged();
    }
}

int AtbAppMenuController::popupEdge() const
{
    return m_popupEdge;
}

void AtbAppMenuController::setPopupEdge(int edge)
{
    if (m_popupEdge != edge) {
        m_popupEdge = edge;
        Q_EMIT popupEdgeChanged();
    }
}

int AtbAppMenuController::currentIndex() const
{
    return m_currentIndex;
}

void AtbAppMenuController::setCurrentIndex(int index)
{
    if (m_currentIndex != index) {
        m_currentIndex = index;
        Q_EMIT currentIndexChanged();
    }
}

void AtbAppMenuController::trigger(QQuickItem *button, int index)
{
    openMenu(button, index, Activation::Explicit);
}

void AtbAppMenuController::openMenu(QQuickItem *button, int index, Activation activation)
{
    if (!m_model || !button || !button->window() || !button->window()->screen()) {
        return;
    }

    QAction *action = m_model->actionAt(index);
    if (!action || !action->isVisible() || !action->isEnabled()) {
        return;
    }

    QMenu *sourceMenu = action == m_sourceAction ? m_sourceMenu.data() : action->menu();
    if (!sourceMenu) {
        // A top level entry without a submenu. Only an explicit activation triggers it,
        // passing over it with the pointer or the arrow keys must not.
        if (activation == Activation::Explicit) {
            closeMenu();
            action->trigger();
        }
        return;
    }

    ensureProxyMenu();
    if (index == m_currentIndex && m_proxyMenu->isVisible()) {
        return;
    }

    // While a menu is shown, its entries are in the proxy.
    const QMenu *content = action == m_sourceAction ? m_proxyMenu.get() : sourceMenu;
    if (content->isEmpty()) {
        // Nothing to show, e.g. the Edit menu of an editor without a document. Like
        // QMenuBar, do not pop up an empty frame: switching keeps the open menu, and an
        // explicit activation closes it and lets the application prepare the entry, in
        // case its menu is only not loaded yet.
        qCDebug(ATB_APPMENU) << "Not opening the empty menu of entry" << index;
        if (activation == Activation::Explicit) {
            closeMenu();
            m_model->refreshMenu(sourceMenu);
        }
        return;
    }
    qCDebug(ATB_APPMENU) << "Opening entry" << index << (activation == Activation::Explicit ? "explicitly" : "by switching") << "instead of"
                         << m_currentIndex;

    // Workaround for QTBUG-59044: when a window that does not accept focus opens a
    // popup that grabs the pointer while a button is pressed, Qt does not see the
    // release and the next click is lost. Release the QML grab manually.
    QTimer::singleShot(0, button, [button] {
        if (QQuickWindow *window = button->window(); window && window->mouseGrabberItem()) {
            window->mouseGrabberItem()->ungrabMouse();
        }
    });

    if (m_sourceAction != action) {
        restoreSourceMenu();
        m_sourceAction = action;
        m_sourceMenu = sourceMenu;
        const auto actions = sourceMenu->actions();
        for (QAction *child : actions) {
            sourceMenu->removeAction(child);
            m_proxyMenu->addAction(child);
        }
        // From now on the top level action returns the proxy from menu() and the proxy
        // returns the top level action from menuAction(), so the importer keeps the
        // visible menu up to date.
        action->setMenu(m_proxyMenu.get());
        watchSubmenus(m_proxyMenu.get());
        if (m_proxyMenu->isVisible()) {
            // Switching the content of an open popup does not emit aboutToShow.
            m_model->refreshMenu(m_proxyMenu.get());
        }
    }

    const QPoint position = popupPosition(button);
    if (m_proxyMenu->isVisible()) {
        // Styles may shift a menu when it is shown, e.g. Kvantum by the size of its shadow,
        // but not when an open menu moves: apply the shift of the last popup.
        m_proxyMenu->move(position + m_styleOffset);
    } else {
        m_proxyMenu->winId(); // create the window handle
        m_proxyMenu->windowHandle()->setTransientParent(button->window());
        m_closedByCompositor = false;
        m_proxyMenu->popup(position);
        // A larger shift is not a shadow but Qt keeping the menu on the screen.
        const QPoint shift = m_proxyMenu->pos() - position;
        m_styleOffset = shift.manhattanLength() <= MaxStyleOffset ? shift : QPoint();
    }

    setCurrentIndex(index);
}

void AtbAppMenuController::closeMenu()
{
    qCDebug(ATB_APPMENU) << "Closing the menu, current" << m_currentIndex << "visible" << (m_proxyMenu && m_proxyMenu->isVisible());
    if (m_proxyMenu && m_proxyMenu->isVisible()) {
        m_proxyMenu->hide();
    }
    // Synchronously: the source menus may be deleted right after this.
    restoreSourceMenu();
    setCurrentIndex(-1);
}

void AtbAppMenuController::ensureProxyMenu()
{
    if (m_proxyMenu) {
        return;
    }
    m_proxyMenu = std::make_unique<QMenu>();
    m_proxyMenu->installEventFilter(this);
    connect(m_proxyMenu.get(), &QMenu::aboutToShow, this, [this] {
        // Let the application update dynamic menus (AboutToShow), like QMenuBar would.
        if (m_model) {
            m_model->refreshMenu(m_proxyMenu.get());
        }
    });
    connect(m_proxyMenu.get(), &QMenu::aboutToHide, this, &AtbAppMenuController::onMenuAboutToHide);
}

void AtbAppMenuController::onMenuAboutToHide()
{
    qCDebug(ATB_APPMENU) << "Menu about to hide, current" << m_currentIndex;
    setCurrentIndex(-1);
    // QMenu emits aboutToHide before it triggers the chosen action, so the menu must not
    // be modified now. Give the actions back once it is really gone, unless it was
    // reopened in the meantime.
    QTimer::singleShot(0, this, [this] {
        if (!m_proxyMenu || !m_proxyMenu->isVisible()) {
            restoreSourceMenu();
            if (!m_closedByCompositor) {
                updatePanelHover();
            }
        }
    });
}

void AtbAppMenuController::scheduleReposition()
{
    if (m_repositionPending) {
        return;
    }
    m_repositionPending = true;
    // Once for a batch of changes.
    QTimer::singleShot(0, this, [this] {
        m_repositionPending = false;
        if (!m_proxyMenu || !m_proxyMenu->isVisible()) {
            return;
        }
        if (m_proxyMenu->isEmpty()) {
            // The application emptied the open menu, e.g. an editor closed its last document.
            closeMenu();
            return;
        }
        if (QQuickItem *button = buttonForIndex(m_currentIndex)) {
            m_proxyMenu->move(popupPosition(button) + m_styleOffset);
        }
    });
}

void AtbAppMenuController::updatePanelHover()
{
    // When a popup grabs the pointer, Qt tells the window below that the pointer left,
    // but not that it is back once the popup is gone, so the panel does not know it is
    // hovered until the pointer moves. Tell it where the pointer is, which Qt tracks
    // from the events of the menu as well. After a click elsewhere, which makes the
    // compositor close the menu, the position is outdated, and the panel was left anyway.
    QQuickWindow *window = m_buttonGrid ? m_buttonGrid->window() : nullptr;
    if (!window || !window->screen()) {
        return;
    }
    const QPointF globalPosition = QCursor::pos(window->screen());
    const QPointF position = window->mapFromGlobal(globalPosition);
    if (!QRectF(QPointF(), window->size()).contains(position)) {
        return;
    }
    QMouseEvent move(QEvent::MouseMove, position, position, globalPosition, Qt::NoButton, Qt::NoButton, QGuiApplication::keyboardModifiers());
    QCoreApplication::sendEvent(window, &move);
}

void AtbAppMenuController::restoreSourceMenu()
{
    if (m_proxyMenu) {
        const auto actions = m_proxyMenu->actions();
        for (QAction *action : actions) {
            m_proxyMenu->removeAction(action);
            if (m_sourceMenu) {
                m_sourceMenu->addAction(action);
            }
        }
        // The importer takes the proxy for the submenu while it is shown, so the entries
        // the application added meanwhile are children of the proxy. They belong to the
        // submenu, or go away with it.
        const auto children = m_proxyMenu->children();
        for (QObject *child : children) {
            auto *submenu = qobject_cast<QMenu *>(child);
            QAction *entry = submenu ? submenu->menuAction() : qobject_cast<QAction *>(child);
            if (!entry || !entry->property(DBusMenuIdProperty).isValid()) {
                continue;
            }
            if (!m_sourceMenu) {
                child->deleteLater();
            } else if (submenu) {
                submenu->setParent(m_sourceMenu.data(), submenu->windowFlags());
            } else {
                child->setParent(m_sourceMenu.data());
            }
        }
    }
    // Without its entry, which the application removed while it was open, the submenu stays
    // with the importer: it may move its entries into the new submenu of a new entry.
    if (m_sourceAction && m_sourceMenu) {
        m_sourceAction->setMenu(m_sourceMenu.data());
    }
    m_sourceAction = nullptr;
    m_sourceMenu = nullptr;
}

QPoint AtbAppMenuController::popupPosition(QQuickItem *button) const
{
    QQuickWindow *window = button->window();
    const QRectF sceneRect = button->mapRectToScene(QRectF(0, 0, button->width(), button->height()));
    const QRect buttonRect(window->mapToGlobal(sceneRect.topLeft().toPoint()), sceneRect.size().toSize());
    const bool rightToLeft = QGuiApplication::layoutDirection() == Qt::RightToLeft;

    m_proxyMenu->setProperty("_breeze_menu_seamless_edges", QVariant::fromValue(Qt::Edges(m_popupEdge)));
    m_proxyMenu->adjustSize();
    const QSize menuSize = m_proxyMenu->size();

    const int alignedX = rightToLeft ? buttonRect.x() + buttonRect.width() - menuSize.width() : buttonRect.x();
    QPoint position;
    switch (m_popupEdge) {
    case Qt::BottomEdge:
        position = QPoint(alignedX, buttonRect.y() - menuSize.height());
        break;
    case Qt::LeftEdge:
        position = QPoint(buttonRect.x() + buttonRect.width(), buttonRect.y());
        break;
    case Qt::RightEdge:
        position = QPoint(buttonRect.x() - menuSize.width(), buttonRect.y());
        break;
    case Qt::TopEdge:
    default:
        position = QPoint(alignedX, buttonRect.y() + buttonRect.height());
        break;
    }

    // Like QMenu itself, keep the menu on the screen of the panel.
    const QRect available = window->screen()->availableGeometry();
    position.setX(qBound(available.x(), position.x(), available.x() + available.width() - menuSize.width()));
    position.setY(qBound(available.y(), position.y(), available.y() + available.height() - menuSize.height()));
    return position;
}

QQuickItem *AtbAppMenuController::buttonForIndex(int index) const
{
    if (!m_buttonGrid) {
        return nullptr;
    }
    const auto children = m_buttonGrid->childItems();
    for (QQuickItem *child : children) {
        bool ok = false;
        if (child->property("buttonIndex").toInt(&ok) == index && ok && child->isVisible()) {
            return child;
        }
    }
    return nullptr;
}

int AtbAppMenuController::neighbourIndex(int step) const
{
    if (!m_model || m_currentIndex < 0) {
        return -1;
    }
    const int count = m_model->count();
    int index = m_currentIndex;
    for (int i = 1; i < count; ++i) {
        index = (index + step + count) % count;
        QAction *action = m_model->actionAt(index);
        // Only entries that can show something are keyboard targets.
        if (action && action->isVisible() && action->isEnabled() && action->menu() && !action->menu()->isEmpty() && !action->text().isEmpty()
            && buttonForIndex(index)) {
            return index;
        }
    }
    return -1;
}

void AtbAppMenuController::switchTo(int index)
{
    if (m_currentIndex < 0 || index < 0 || index == m_currentIndex) {
        return;
    }
    if (QQuickItem *button = buttonForIndex(index)) {
        openMenu(button, index, Activation::Switch);
    }
}

void AtbAppMenuController::watchSubmenus(QMenu *menu)
{
    const auto actions = menu->actions();
    for (QAction *action : actions) {
        if (QMenu *submenu = action->menu()) {
            // Installing the filter again does not install it twice.
            submenu->installEventFilter(this);
        }
    }
}

bool AtbAppMenuController::eventFilter(QObject *watched, QEvent *event)
{
    auto *menu = qobject_cast<QMenu *>(watched);
    if (!menu) {
        return false;
    }

    if (event->type() == QEvent::Close) {
        // Escape, a click outside or the compositor close a popup, and closing a window
        // destroys its native surface, unlike hiding it. KWindowSystem (6.30 at least)
        // can then keep a blur object of the destroyed surface, created when the style
        // switched the blur off (Kvantum does that for menus too small to have a blur
        // region), and uses it once the blur is switched on again. That is a fatal
        // Wayland protocol error, which takes the whole plasmashell down, see
        // https://bugs.kde.org/show_bug.cgi?id=522547. Hide our menus instead, so their
        // surfaces live as long as the menus themselves.
        qCDebug(ATB_APPMENU) << "Hiding instead of closing" << (menu == m_proxyMenu.get() ? "the menu" : "a submenu");
        if (menu == m_proxyMenu.get()) {
            // A spontaneous close comes from the compositor, not from QMenu itself.
            m_closedByCompositor = event->spontaneous();
        }
        event->ignore();
        menu->hide();
        return true;
    }
    if (event->type() == QEvent::Show) {
        watchSubmenus(menu);
    } else if (event->type() == QEvent::ActionAdded) {
        // Also the submenus of entries that appear while the menu is shown.
        if (QMenu *submenu = static_cast<QActionEvent *>(event)->action()->menu()) {
            submenu->installEventFilter(this);
        }
    }

    if (menu != m_proxyMenu.get()) {
        return false;
    }

    if (event->type() == QEvent::DeferredDelete) {
        // The importer deletes the submenu of an entry that the application removed, and
        // while the entry is open, that is the proxy, which belongs to this controller.
        return true;
    }

    switch (event->type()) {
    case QEvent::ActionAdded:
    case QEvent::ActionRemoved:
    case QEvent::ActionChanged:
        // The entries of the open menu changed, e.g. search results were added: fit it
        // to them at its button. Qt only resizes it, and on Wayland not reliably.
        if (m_proxyMenu->isVisible()) {
            scheduleReposition();
        }
        break;
    default:
        break;
    }

    if (event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        const bool rightToLeft = m_proxyMenu->layoutDirection() == Qt::RightToLeft;
        const int forwardKey = rightToLeft ? Qt::Key_Left : Qt::Key_Right;
        const int backwardKey = rightToLeft ? Qt::Key_Right : Qt::Key_Left;

        if (keyEvent->key() == backwardKey) {
            switchTo(neighbourIndex(-1));
            return true;
        }
        if (keyEvent->key() == forwardKey) {
            if (QAction *active = m_proxyMenu->activeAction(); active && active->menu()) {
                return false; // let QMenu open the submenu
            }
            switchTo(neighbourIndex(1));
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        // While the popup grabs the pointer, the buttons of the menu bar do not see
        // hover events, so switch menus from here.
        if (!m_buttonGrid || !m_buttonGrid->window()) {
            return false;
        }
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPointF windowPos = m_buttonGrid->window()->mapFromGlobal(mouseEvent->globalPosition());
        const QPointF gridPos = m_buttonGrid->mapFromScene(windowPos);
        if (QQuickItem *item = m_buttonGrid->childAt(gridPos.x(), gridPos.y())) {
            bool ok = false;
            const int index = item->property("buttonIndex").toInt(&ok);
            if (ok && index != m_currentIndex) {
                qCDebug(ATB_APPMENU) << "Pointer moved to entry" << index;
                switchTo(index);
            }
        }
    }

    return false;
}

#include "moc_appmenucontroller.cpp"
