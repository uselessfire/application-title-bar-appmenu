/*
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "appmenucontroller.h"
#include "appmenumodel.h"
#include "fakemenuexporter.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QWidgetAction>

#include <memory>

namespace
{
const QString ViewService = QStringLiteral("org.kde.kappmenuview");
const QString MenuPath = QStringLiteral("/MenuBar/1");

FakeMenuExporter::Item makeItem(int id, const QString &label, const QList<FakeMenuExporter::Item> &children = {})
{
    FakeMenuExporter::Item item;
    item.id = id;
    item.label = label;
    item.submenu = !children.isEmpty();
    item.children = children;
    return item;
}

FakeMenuExporter::Item defaultMenu()
{
    FakeMenuExporter::Item root;
    root.children = {
        makeItem(1, QStringLiteral("_File"), {makeItem(11, QStringLiteral("_Open")), makeItem(12, QStringLiteral("_Quit"))}),
        makeItem(2, QStringLiteral("_Edit"), {makeItem(21, QStringLiteral("_Copy"))}),
        makeItem(3, QStringLiteral("Plain")),
    };
    return root;
}

QString viewServiceOwner()
{
    return QDBusConnection::sessionBus().interface()->serviceOwner(ViewService).value();
}

bool viewServiceRegistered()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(ViewService).value();
}

QString rowText(const AtbAppMenuModel &model, int row)
{
    return model.data(model.index(row), AtbAppMenuModel::MenuRole).toString();
}

QPointF globalCenter(QQuickItem *item)
{
    return item->window()->mapToGlobal(item->mapToScene(QPointF(item->width() / 2, item->height() / 2)));
}

void sendMouseMove(QWidget *widget, const QPointF &globalPos)
{
    QMouseEvent move(QEvent::MouseMove, widget->mapFromGlobal(globalPos), globalPos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &move);
}

// Moves menus when they are shown, like Kvantum does by the size of its shadows.
class ShadowStyle : public QObject
{
public:
    static constexpr QPoint Shift{-6, -3};

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Show) {
            auto *widget = static_cast<QWidget *>(watched);
            widget->move(widget->pos() + Shift);
        }
        return false;
    }
};

// Records the mouse events a window gets.
class MouseEventRecorder : public QObject
{
public:
    QList<QPointF> moves;
    int presses = 0;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (event->type() == QEvent::MouseMove) {
            moves.append(static_cast<QMouseEvent *>(event)->position());
        } else if (event->type() == QEvent::MouseButtonPress) {
            ++presses;
        }
        return false;
    }
};

// The importer ignores the first update of a layout that it has just refreshed itself.
void emitLayoutUpdatedTwice(FakeMenuExporter &exporter, int parentId)
{
    exporter.emitLayoutUpdated(parentId);
    QTest::qWait(100);
    exporter.emitLayoutUpdated(parentId);
}

// A window with a row of buttons, like the menu bar element of the widget.
struct ButtonBar {
    explicit ButtonBar(int count)
    {
        window.resize(count * 100, 40);
        grid = new QQuickItem(window.contentItem());
        grid->setSize(QSizeF(count * 100, 40));
        for (int i = 0; i < count; ++i) {
            auto *button = new QQuickItem(grid);
            button->setProperty("buttonIndex", i);
            button->setPosition(QPointF(i * 100, 0));
            button->setSize(QSizeF(100, 40));
            buttons.append(button);
        }
        window.show();
    }

    QQuickWindow window;
    QQuickItem *grid = nullptr;
    QList<QQuickItem *> buttons;
};
}

class AppMenuTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void topLevelEntries();
    void unchangedAddressIsNoop();
    void layoutUpdate();
    void activationRequest();
    void propertiesUpdate();
    void clearMenu();
    void serviceGone();
    void searchEntry();
    void controllerOpensAndSwitches();
    void controllerTriggersPlainEntryOnlyExplicitly();
    void controllerSkipsEmptyMenus();
    void controllerKeepsStyleOffset();
    void controllerRestoresPanelHover();
    void controllerShowsSearchResults();
    void controllerFocusesSearchField();
    void controllerSwitchesFromSearchField();
    void controllerTriggersSearchResultOnce();
    void controllerAdoptsEntriesAddedWhileOpen();
    void controllerSurvivesRebuildOfOpenEntry();
    void controllerClosesMenuEmptiedWhileOpen();
    void controllerClosesOnClickOnItsButton();
    void controllerHidesInsteadOfClosing();
    void controllerClosesWhenMenuIsReplaced();
    void viewServiceRegistration();
    void viewServiceSharedWithAnotherOwner();

private:
    void setUpModel(AtbAppMenuModel &model);

    std::unique_ptr<FakeMenuExporter> m_exporter;
    int m_exporterCount = 0;
};

void AppMenuTest::initTestCase()
{
    // The controller registers org.kde.kappmenuview, which switches the global menu
    // on for the whole session. Never do that on the user's session bus.
    if (!qEnvironmentVariableIsSet("ATB_APPMENU_TEST_PRIVATE_BUS")) {
        QSKIP("Run through ctest, the test needs a private session bus (dbus-run-session)");
    }
    QVERIFY(QDBusConnection::sessionBus().isConnected());
}

void AppMenuTest::init()
{
    m_exporter = std::make_unique<FakeMenuExporter>(QStringLiteral("exporter-%1").arg(++m_exporterCount), MenuPath);
    m_exporter->setRoot(defaultMenu());
}

void AppMenuTest::cleanup()
{
    m_exporter.reset();
    QTRY_VERIFY(!viewServiceRegistered());
}

void AppMenuTest::setUpModel(AtbAppMenuModel &model)
{
    model.setMenu(m_exporter->service(), m_exporter->path());
    QTRY_COMPARE(model.count(), 3);
    // The first level of submenus is cached right away.
    QTRY_COMPARE(model.actionAt(0)->menu()->actions().size(), 2);
    QTRY_COMPARE(model.actionAt(1)->menu()->actions().size(), 1);
}

void AppMenuTest::topLevelEntries()
{
    AtbAppMenuModel model;
    QVERIFY(!model.menuAvailable());

    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    QVERIFY(model.menuAvailable());
    QCOMPARE(model.serviceName(), m_exporter->service());
    QCOMPARE(model.menuObjectPath(), MenuPath);
    QCOMPARE(rowText(model, 0), QStringLiteral("&File"));
    QCOMPARE(rowText(model, 1), QStringLiteral("&Edit"));
    QCOMPARE(rowText(model, 2), QStringLiteral("Plain"));
    QVERIFY(model.actionAt(0)->menu());
    QVERIFY(!model.actionAt(2)->menu());
    QVERIFY(m_exporter->aboutToShowCalls.contains(0));
}

void AppMenuTest::unchangedAddressIsNoop()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }
    QTest::qWait(200);

    const auto aboutToShowCalls = m_exporter->aboutToShowCalls.size();
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy menuChanged(&model, &AtbAppMenuModel::menuChanged);

    model.setMenu(m_exporter->service(), m_exporter->path());
    QTest::qWait(200);

    QCOMPARE(m_exporter->aboutToShowCalls.size(), aboutToShowCalls);
    QCOMPARE(resets.count(), 0);
    QCOMPARE(menuChanged.count(), 0);
}

void AppMenuTest::layoutUpdate()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    m_exporter->root().children.append(makeItem(4, QStringLiteral("_View"), {makeItem(41, QStringLiteral("_Zoom"))}));
    m_exporter->emitLayoutUpdated(0);
    QTRY_COMPARE(model.count(), 4);
    QCOMPARE(rowText(model, 3), QStringLiteral("&View"));

    m_exporter->root().children.removeFirst();
    m_exporter->emitLayoutUpdated(0);
    QTRY_COMPARE(model.count(), 3);
    QCOMPARE(rowText(model, 0), QStringLiteral("&Edit"));
}

void AppMenuTest::activationRequest()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }
    QSignalSpy requests(&model, &AtbAppMenuModel::requestActivateIndex);

    m_exporter->emitItemActivationRequested(2); // Edit
    QTRY_COMPARE(requests.count(), 1);
    QCOMPARE(requests.at(0).at(0).toInt(), 1);

    m_exporter->emitItemActivationRequested(12); // File > Quit opens File
    QTRY_COMPARE(requests.count(), 2);
    QCOMPARE(requests.at(1).at(0).toInt(), 0);
}

void AppMenuTest::propertiesUpdate()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }
    QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

    // Applications report changed labels or states of existing entries this way.
    AtbDBusMenu::DBusMenuItem edit;
    edit.id = 2;
    edit.properties.insert(QStringLiteral("label"), QStringLiteral("_Change"));
    edit.properties.insert(QStringLiteral("enabled"), false);
    m_exporter->emitItemsPropertiesUpdated({edit});

    QTRY_COMPARE(rowText(model, 1), QStringLiteral("&Change"));
    QVERIFY(!model.actionAt(1)->isEnabled());
    QVERIFY(!dataChanged.isEmpty());
    QCOMPARE(dataChanged.constLast().at(0).toModelIndex().row(), 1);
}

void AppMenuTest::clearMenu()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    model.setMenu(QString(), QString());
    QTRY_COMPARE(model.count(), 0);
    QVERIFY(!model.menuAvailable());
    QVERIFY(model.serviceName().isEmpty());
}

void AppMenuTest::serviceGone()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    m_exporter->disconnectFromBus();
    QTRY_COMPARE(model.count(), 0);
    QVERIFY(!model.menuAvailable());
    QVERIFY(model.serviceName().isEmpty());
}

void AppMenuTest::searchEntry()
{
    // An accelerator inside of a word.
    m_exporter->root().children[1].children.append(makeItem(22, QStringLiteral("Pa_ste")));

    AtbAppMenuModel model;
    model.setSearchEnabled(true);
    model.setMenu(m_exporter->service(), m_exporter->path());
    QTRY_COMPARE(model.count(), 4);
    QTRY_COMPARE(model.actionAt(1)->menu()->actions().size(), 2);

    QAction *search = model.actionAt(3);
    QCOMPARE(search->objectName(), QStringLiteral("appmenu-search"));
    QVERIFY(search->menu());

    auto *fieldAction = qobject_cast<QWidgetAction *>(search->menu()->actions().constFirst());
    QVERIFY(fieldAction);
    auto *field = qobject_cast<QLineEdit *>(fieldAction->defaultWidget());
    QVERIFY(field);
    field->setText(QStringLiteral("cop"));
    const auto results = search->menu()->actions();
    QVERIFY(std::any_of(results.cbegin(), results.cend(), [](QAction *action) {
        return action->text() == QStringLiteral("&Copy");
    }));

    // The accelerator marker is not part of the text that is searched.
    field->setText(QStringLiteral("paste"));
    const auto pasteResults = search->menu()->actions();
    QVERIFY(std::any_of(pasteResults.cbegin(), pasteResults.cend(), [](QAction *action) {
        return action->text() == QStringLiteral("Pa&ste");
    }));

    model.setSearchEnabled(false);
    QTRY_COMPARE(model.count(), 3);
}

void AppMenuTest::controllerOpensAndSwitches()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    ButtonBar bar(3);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));
    const auto &buttons = bar.buttons;

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);
    controller.setPopupEdge(Qt::TopEdge);

    QAction *file = model.actionAt(0);
    QMenu *fileMenu = file->menu();
    const auto aboutToShowFile = m_exporter->aboutToShowCalls.count(1);

    controller.trigger(buttons.at(0), 0);
    QCOMPARE(controller.currentIndex(), 0);
    auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    QVERIFY(popup != fileMenu);
    QCOMPARE(popup->actions().size(), 2);
    // While it is open, the top level action points at the popup, so updates reach it.
    QCOMPARE(file->menu(), popup);
    // Opening a menu lets the application prepare it.
    QTRY_VERIFY(m_exporter->aboutToShowCalls.count(1) > aboutToShowFile);

    // The arrow keys move between entries that have a submenu and skip "Plain".
    QTest::keyClick(popup, Qt::Key_Right);
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(popup->actions().size(), 1);
    QCOMPARE(file->menu(), fileMenu);
    QCOMPARE(fileMenu->actions().size(), 2);
    QTest::keyClick(popup, Qt::Key_Right);
    QCOMPARE(controller.currentIndex(), 0);
    QTest::keyClick(popup, Qt::Key_Left);
    QCOMPARE(controller.currentIndex(), 1);

    // Moving the pointer over another button switches as well...
    sendMouseMove(popup, globalCenter(buttons.at(0)));
    QCOMPARE(controller.currentIndex(), 0);
    // ...but passing over an entry without submenu neither opens nor triggers it,
    // whether the menu or the button sees the pointer.
    sendMouseMove(popup, globalCenter(buttons.at(2)));
    QCOMPARE(controller.currentIndex(), 0);
    controller.switchTo(2);
    QCOMPARE(controller.currentIndex(), 0);
    QTest::qWait(100);
    QVERIFY(!m_exporter->events.contains(qMakePair(3, QStringLiteral("clicked"))));
    controller.switchTo(1);
    QCOMPARE(controller.currentIndex(), 1);

    controller.closeMenu();
    QCOMPARE(controller.currentIndex(), -1);
    QVERIFY(!popup->isVisible());
    QCOMPARE(file->menu(), fileMenu);
    QCOMPARE(fileMenu->actions().size(), 2);

    // Switching only makes sense while a menu is open.
    controller.switchTo(1);
    QCOMPARE(controller.currentIndex(), -1);
    QVERIFY(!popup->isVisible());
}

void AppMenuTest::controllerSkipsEmptyMenus()
{
    // Like the Edit menu of an editor without a document: all of its entries are hidden.
    FakeMenuExporter::Item hidden = makeItem(21, QStringLiteral("_Copy"));
    hidden.visible = false;
    m_exporter->root().children[1].children = {hidden};

    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    ButtonBar bar(3);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    // No empty frame pops up, but the application may prepare the menu.
    const auto aboutToShowEdit = m_exporter->aboutToShowCalls.count(2);
    controller.trigger(bar.buttons.at(1), 1);
    QCOMPARE(controller.currentIndex(), -1);
    QVERIFY(!QApplication::activePopupWidget());
    QTRY_VERIFY(m_exporter->aboutToShowCalls.count(2) > aboutToShowEdit);

    // An open menu stays when the pointer or the arrow keys pass over the empty one.
    controller.trigger(bar.buttons.at(0), 0);
    auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    controller.switchTo(1);
    QCOMPARE(controller.currentIndex(), 0);
    QTest::keyClick(popup, Qt::Key_Right);
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(popup->actions().size(), 2);
    QVERIFY(popup->isVisible());

    // Clicking the empty entry closes the open menu, like in QMenuBar.
    controller.trigger(bar.buttons.at(1), 1);
    QCOMPARE(controller.currentIndex(), -1);
    QVERIFY(!popup->isVisible());
}

void AppMenuTest::controllerKeepsStyleOffset()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    ButtonBar bar(3);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);
    controller.setPopupEdge(Qt::TopEdge);

    controller.trigger(bar.buttons.at(0), 0);
    QPointer<QMenu> popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    const QPoint filePosition = popup->pos();
    controller.closeMenu();

    // The style moves the menu when it is shown...
    ShadowStyle style;
    popup->installEventFilter(&style);
    controller.trigger(bar.buttons.at(0), 0);
    QCOMPARE(popup->pos(), filePosition + ShadowStyle::Shift);

    // ...but not when it moves to another button, so the controller does it.
    controller.switchTo(1);
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(popup->pos(), filePosition + QPoint(100, 0) + ShadowStyle::Shift);
    controller.closeMenu();
}

void AppMenuTest::controllerRestoresPanelHover()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    ButtonBar bar(3);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));
    MouseEventRecorder recorder;
    bar.window.installEventFilter(&recorder);

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    // Escape closes the menu while the pointer rests on the panel: the panel learns
    // where the pointer is, which it missed while the menu had it.
    const QPointF onButton = globalCenter(bar.buttons.at(0));
    QCursor::setPos(onButton.toPoint());
    controller.trigger(bar.buttons.at(0), 0);
    QPointer<QMenu> popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    recorder.moves.clear();
    popup->close();
    QTRY_VERIFY(!recorder.moves.isEmpty());
    QCOMPARE(recorder.moves.constLast(), bar.window.mapFromGlobal(onButton));

    // Nothing is sent when the pointer is elsewhere.
    QCursor::setPos(bar.window.mapToGlobal(QPoint(-100, -100)));
    controller.trigger(bar.buttons.at(0), 0);
    QVERIFY(popup->isVisible());
    recorder.moves.clear();
    popup->close();
    QTest::qWait(50);
    QVERIFY(recorder.moves.isEmpty());
}

void AppMenuTest::controllerShowsSearchResults()
{
    AtbAppMenuModel model;
    model.setSearchEnabled(true);
    model.setMenu(m_exporter->service(), m_exporter->path());
    QTRY_COMPARE(model.count(), 4);
    QTRY_COMPARE(model.actionAt(1)->menu()->actions().size(), 1);

    // A panel at the bottom of the screen: its menus grow upwards.
    ButtonBar bar(4);
    bar.window.setPosition(0, 400);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));
    const int panelTop = bar.window.mapToGlobal(QPoint(0, 0)).y();

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);
    controller.setPopupEdge(Qt::BottomEdge);

    controller.trigger(bar.buttons.at(3), 3);
    auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    auto *fieldAction = qobject_cast<QWidgetAction *>(popup->actions().constFirst());
    QVERIFY(fieldAction);
    auto *field = qobject_cast<QLineEdit *>(fieldAction->defaultWidget());
    QVERIFY(field);

    QCOMPARE(popup->geometry().bottom() + 1, panelTop);
    const int searchHeight = popup->height();

    // The results show up in the open menu, which grows and stays at the panel.
    field->setText(QStringLiteral("cop"));
    const auto shown = popup->actions();
    QVERIFY(std::any_of(shown.cbegin(), shown.cend(), [](QAction *action) {
        return action->text() == QStringLiteral("&Copy");
    }));
    QTRY_VERIFY(popup->height() > searchHeight);
    QTRY_COMPARE(popup->geometry().bottom() + 1, panelTop);

    field->clear();
    QCOMPARE(popup->actions().size(), 2); // the field and the separator
    QTRY_COMPARE(popup->height(), searchHeight);
    QTRY_COMPARE(popup->geometry().bottom() + 1, panelTop);

    controller.closeMenu();
}

void AppMenuTest::controllerFocusesSearchField()
{
    AtbAppMenuModel model;
    model.setSearchEnabled(true);
    model.setMenu(m_exporter->service(), m_exporter->path());
    QTRY_COMPARE(model.count(), 4);
    QTRY_COMPARE(model.actionAt(1)->menu()->actions().size(), 1);

    ButtonBar bar(4);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    // Opened with a click: typing goes to the field, not to the keyboard navigation of the menu.
    controller.trigger(bar.buttons.at(3), 3);
    auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    auto *field = popup->findChild<QLineEdit *>();
    QVERIFY(field);
    QTRY_COMPARE(QApplication::focusWidget(), field);
    QTest::keyClick(popup->windowHandle(), 'c');
    QTest::keyClick(popup->windowHandle(), 'o');
    QCOMPARE(field->text(), QStringLiteral("co"));
    const auto shown = popup->actions();
    QVERIFY(std::any_of(shown.cbegin(), shown.cend(), [](QAction *action) {
        return action->text() == QStringLiteral("&Copy");
    }));
    field->clear();

    // Reached by switching from another open menu: the same.
    controller.switchTo(0);
    QCOMPARE(controller.currentIndex(), 0);
    controller.switchTo(3);
    QCOMPARE(controller.currentIndex(), 3);
    QTRY_COMPARE(QApplication::focusWidget(), field);

    // Opened again, the last query is selected with its results: typing replaces it.
    field->setText(QStringLiteral("co"));
    controller.closeMenu();
    controller.trigger(bar.buttons.at(3), 3);
    popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    QTRY_COMPARE(QApplication::focusWidget(), field);
    QCOMPARE(field->selectedText(), QStringLiteral("co"));
    QTest::keyClick(popup->windowHandle(), 'q');
    QCOMPARE(field->text(), QStringLiteral("q"));

    controller.closeMenu();
}

void AppMenuTest::controllerSwitchesFromSearchField()
{
    AtbAppMenuModel model;
    model.setSearchEnabled(true);
    model.setMenu(m_exporter->service(), m_exporter->path());
    QTRY_COMPARE(model.count(), 4);
    QTRY_COMPARE(model.actionAt(1)->menu()->actions().size(), 1);

    ButtonBar bar(4);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    controller.trigger(bar.buttons.at(3), 3);
    auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    auto *field = popup->findChild<QLineEdit *>();
    QVERIFY(field);
    QTRY_COMPARE(QApplication::focusWidget(), field);

    // In the empty field, the arrow keys switch menus like everywhere else: Left skips
    // the entry without a submenu, and Right comes back to the search entry.
    QTest::keyClick(popup->windowHandle(), Qt::Key_Left);
    QCOMPARE(controller.currentIndex(), 1);
    QTest::keyClick(popup->windowHandle(), Qt::Key_Right);
    QCOMPARE(controller.currentIndex(), 3);
    QTRY_COMPARE(QApplication::focusWidget(), field);
    QTest::keyClick(popup->windowHandle(), Qt::Key_Right);
    QCOMPARE(controller.currentIndex(), 0);
    QTest::keyClick(popup->windowHandle(), Qt::Key_Left);
    QCOMPARE(controller.currentIndex(), 3);

    // Within the text they move the cursor, at its ends they switch menus.
    QTest::keyClick(popup->windowHandle(), 'c');
    QTest::keyClick(popup->windowHandle(), 'o');
    QTest::keyClick(popup->windowHandle(), Qt::Key_Left);
    QCOMPARE(controller.currentIndex(), 3);
    QCOMPARE(field->cursorPosition(), 1);
    QTest::keyClick(popup->windowHandle(), Qt::Key_Left);
    QCOMPARE(controller.currentIndex(), 3);
    QCOMPARE(field->cursorPosition(), 0);
    QTest::keyClick(popup->windowHandle(), Qt::Key_Left);
    QCOMPARE(controller.currentIndex(), 1);

    controller.closeMenu();
}

void AppMenuTest::controllerTriggersSearchResultOnce()
{
    AtbAppMenuModel model;
    model.setSearchEnabled(true);
    model.setMenu(m_exporter->service(), m_exporter->path());
    QTRY_COMPARE(model.count(), 4);
    QTRY_COMPARE(model.actionAt(1)->menu()->actions().size(), 1);

    ButtonBar bar(4);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    const auto clicks = [this](int id) {
        return std::count(m_exporter->events.cbegin(), m_exporter->events.cend(), qMakePair(id, QStringLiteral("clicked")));
    };
    const auto openSearch = [&]() -> QMenu * {
        controller.trigger(bar.buttons.at(3), 3);
        return qobject_cast<QMenu *>(QApplication::activePopupWidget());
    };

    // Without results, Return does nothing and the menu stays open.
    QMenu *popup = openSearch();
    QVERIFY(popup);
    auto *field = popup->findChild<QLineEdit *>();
    QVERIFY(field);
    QTRY_COMPARE(QApplication::focusWidget(), field);
    QTest::keyClick(popup->windowHandle(), 'x');
    QTest::keyClick(popup->windowHandle(), Qt::Key_Return);
    QVERIFY(popup->isVisible());
    QCOMPARE(controller.currentIndex(), 3);

    // Otherwise it triggers the first result, once.
    field->setText(QStringLiteral("o"));
    QTest::keyClick(popup->windowHandle(), Qt::Key_Return);
    QTRY_VERIFY(!popup->isVisible());
    QTRY_COMPARE(clicks(11), 1); // Open
    QTest::qWait(100);
    QCOMPARE(clicks(11) + clicks(21), 1);

    // Or the result under the pointer, once as well.
    popup = openSearch();
    QVERIFY(popup);
    QTRY_COMPARE(QApplication::focusWidget(), field);
    field->setText(QStringLiteral("co"));
    const auto shown = popup->actions();
    const auto copy = std::find_if(shown.cbegin(), shown.cend(), [](QAction *action) {
        return action->text() == QStringLiteral("&Copy");
    });
    QVERIFY(copy != shown.cend());
    popup->setActiveAction(*copy);
    QTest::keyClick(popup->windowHandle(), Qt::Key_Return);
    QTRY_VERIFY(!popup->isVisible());
    QTRY_COMPARE(clicks(21), 1);
    QTest::qWait(100);
    QCOMPARE(clicks(21), 1);
    QCOMPARE(clicks(11), 1);
}

void AppMenuTest::controllerAdoptsEntriesAddedWhileOpen()
{
    AtbAppMenuModel model;
    model.setSearchEnabled(true);
    model.setMenu(m_exporter->service(), m_exporter->path());
    QTRY_COMPARE(model.count(), 4);
    QTRY_COMPARE(model.actionAt(0)->menu()->actions().size(), 2);

    ButtonBar bar(4);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    QPointer<QMenu> fileMenu = model.actionAt(0)->menu();
    controller.trigger(bar.buttons.at(0), 0);
    QPointer<QMenu> popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);

    // The application adds an entry and a submenu to the open File menu.
    m_exporter->root().children[0].children.append(makeItem(13, QStringLiteral("_Export")));
    m_exporter->root().children[0].children.append(makeItem(14, QStringLiteral("_Recent"), {makeItem(141, QStringLiteral("notes.txt"))}));
    emitLayoutUpdatedTwice(*m_exporter, 1);
    QTRY_COMPARE(popup->actions().size(), 4);
    QPointer<QAction> exportEntry = popup->actions().at(2);
    QPointer<QMenu> recentMenu = popup->actions().at(3)->menu();
    QVERIFY(exportEntry && recentMenu);

    // Once the menu is closed, they belong to the File menu like the other entries.
    controller.closeMenu();
    QTRY_COMPARE(fileMenu->actions().size(), 4);
    QCOMPARE(exportEntry->parent(), fileMenu.data());
    QCOMPARE(recentMenu->parent(), fileMenu.data());
    QCOMPARE(recentMenu->windowType(), Qt::Popup);

    // The application can open the menu of the new entry, and the search finds it.
    QSignalSpy requests(&model, &AtbAppMenuModel::requestActivateIndex);
    m_exporter->emitItemActivationRequested(13);
    QTRY_COMPARE(requests.count(), 1);
    QCOMPARE(requests.at(0).at(0).toInt(), 0);

    QMenu *searchMenu = model.actionAt(3)->menu();
    auto *field = qobject_cast<QLineEdit *>(qobject_cast<QWidgetAction *>(searchMenu->actions().constFirst())->defaultWidget());
    QVERIFY(field);
    field->setText(QStringLiteral("export"));
    const auto results = searchMenu->actions();
    QVERIFY(std::any_of(results.cbegin(), results.cend(), [](QAction *action) {
        return action->text() == QStringLiteral("&Export");
    }));
    field->clear();

    // And they go away with the menu of the application.
    model.setMenu(QString(), QString());
    QTRY_VERIFY(exportEntry.isNull());
    QTRY_VERIFY(recentMenu.isNull());
    QVERIFY(popup);
}

void AppMenuTest::controllerSurvivesRebuildOfOpenEntry()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    ButtonBar bar(3);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    auto controller = std::make_unique<AtbAppMenuController>();
    controller->setModel(&model);
    controller->setButtonGrid(bar.grid);

    controller->trigger(bar.buttons.at(0), 0);
    QPointer<QMenu> popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);

    // The application rebuilds its menu bar while File is open, and File gets another id.
    // The importer deletes the old entry and what it takes for its submenu: the popup.
    m_exporter->root().children[0].id = 100;
    emitLayoutUpdatedTwice(*m_exporter, 0);
    QTRY_COMPARE(model.actionAt(0)->property("_dbusmenu_id").toInt(), 100);
    QTRY_COMPARE(controller->currentIndex(), -1);
    QVERIFY(popup);
    QVERIFY(!popup->isVisible());

    // The new File menu opens in the same popup.
    QTRY_COMPARE(model.actionAt(0)->menu()->actions().size(), 2);
    controller->trigger(bar.buttons.at(0), 0);
    QCOMPARE(controller->currentIndex(), 0);
    QCOMPARE(QApplication::activePopupWidget(), popup.data());
    QCOMPARE(popup->actions().size(), 2);

    controller.reset();
    QVERIFY(popup.isNull());
}

void AppMenuTest::controllerClosesMenuEmptiedWhileOpen()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    ButtonBar bar(3);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    controller.trigger(bar.buttons.at(0), 0);
    QPointer<QMenu> popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);

    // Like an editor that closes its last document: no empty frame stays open.
    m_exporter->root().children[0].children.clear();
    emitLayoutUpdatedTwice(*m_exporter, 1);
    QTRY_COMPARE(controller.currentIndex(), -1);
    QVERIFY(!popup->isVisible());
}

void AppMenuTest::controllerClosesOnClickOnItsButton()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    ButtonBar bar(3);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));
    MouseEventRecorder recorder;
    bar.window.installEventFilter(&recorder);

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    controller.trigger(bar.buttons.at(0), 0);
    QPointer<QMenu> popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);

    // The press that closes the menu goes to the menu, not to the panel, so the button
    // under the pointer cannot open the menu again.
    const QPoint onButton = bar.window.mapFromGlobal(globalCenter(bar.buttons.at(0))).toPoint();
    QTest::mouseClick(&bar.window, Qt::LeftButton, {}, onButton);
    QTRY_VERIFY(!popup->isVisible());
    QCOMPARE(controller.currentIndex(), -1);
    QCOMPARE(recorder.presses, 0);
}

void AppMenuTest::controllerHidesInsteadOfClosing()
{
    // File > Recent is a submenu of the shown menu.
    m_exporter->root().children[0].children.append(makeItem(13, QStringLiteral("_Recent"), {makeItem(131, QStringLiteral("notes.txt"))}));

    AtbAppMenuModel model;
    model.setMenu(m_exporter->service(), m_exporter->path());
    QTRY_COMPARE(model.count(), 3);
    QTRY_COMPARE(model.actionAt(0)->menu()->actions().size(), 3);

    ButtonBar bar(3);
    QVERIFY(QTest::qWaitForWindowExposed(&bar.window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.setButtonGrid(bar.grid);

    controller.trigger(bar.buttons.at(0), 0);
    QPointer<QMenu> popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);
    QVERIFY(popup->windowHandle() && popup->windowHandle()->handle());
    const auto *nativeWindow = popup->windowHandle()->handle();

    // Escape, a click outside and the compositor close the popup. That must not
    // destroy its native window, i.e. its Wayland surface (KDE bug 522547).
    popup->close();
    QVERIFY(popup);
    QVERIFY(!popup->isVisible());
    QCOMPARE(controller.currentIndex(), -1);
    QCOMPARE(popup->windowHandle()->handle(), nativeWindow);

    // It comes back in the same window.
    controller.trigger(bar.buttons.at(0), 0);
    QCOMPARE(QApplication::activePopupWidget(), popup.data());
    QCOMPARE(popup->windowHandle()->handle(), nativeWindow);

    // The same holds for its submenus, which are loaded when they are about to show.
    QMenu *recent = popup->actions().at(2)->menu();
    QVERIFY(recent);
    model.refreshMenu(recent);
    QTRY_COMPARE(recent->actions().size(), 1);
    recent->popup(QPoint(0, 0));
    QVERIFY(recent->isVisible());
    const auto *recentNativeWindow = recent->windowHandle()->handle();
    QVERIFY(recentNativeWindow);
    recent->close();
    QVERIFY(!recent->isVisible());
    QCOMPARE(recent->windowHandle()->handle(), recentNativeWindow);

    controller.closeMenu();
}

void AppMenuTest::controllerTriggersPlainEntryOnlyExplicitly()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    QQuickWindow window;
    auto *button = new QQuickItem(window.contentItem());
    button->setSize(QSizeF(100, 40));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.trigger(button, 2);
    QCOMPARE(controller.currentIndex(), -1);
    QTRY_VERIFY(m_exporter->events.contains(qMakePair(3, QStringLiteral("clicked"))));
}

void AppMenuTest::controllerClosesWhenMenuIsReplaced()
{
    AtbAppMenuModel model;
    setUpModel(model);
    if (QTest::currentTestFailed()) {
        return;
    }

    QQuickWindow window;
    auto *button = new QQuickItem(window.contentItem());
    button->setSize(QSizeF(100, 40));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    AtbAppMenuController controller;
    controller.setModel(&model);
    controller.trigger(button, 0);
    QCOMPARE(controller.currentIndex(), 0);
    QPointer<QMenu> popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QVERIFY(popup);

    // Another window becomes active while the menu is open.
    model.setMenu(QString(), QString());
    QCOMPARE(controller.currentIndex(), -1);
    QVERIFY(!popup || !popup->isVisible());
    QTRY_COMPARE(model.count(), 0);
}

void AppMenuTest::viewServiceRegistration()
{
    QVERIFY(!viewServiceRegistered());
    {
        auto first = std::make_unique<AtbAppMenuController>();
        QTRY_VERIFY(viewServiceRegistered());
        auto second = std::make_unique<AtbAppMenuController>();
        first.reset();
        // Still in use by the second controller.
        QTest::qWait(100);
        QVERIFY(viewServiceRegistered());
    }
    QTRY_VERIFY(!viewServiceRegistered());
}

void AppMenuTest::viewServiceSharedWithAnotherOwner()
{
    // Another Global Menu widget in the same process owns the name on the shared connection.
    const QString otherName = QStringLiteral("other-view-owner");
    QDBusConnection other = QDBusConnection::connectToBus(QDBusConnection::SessionBus, otherName);
    other.interface()->registerService(ViewService, QDBusConnectionInterface::QueueService, QDBusConnectionInterface::DontAllowReplacement);
    QTRY_COMPARE(viewServiceOwner(), other.baseService());

    {
        AtbAppMenuController controller;
        QTest::qWait(100);
        QCOMPARE(viewServiceOwner(), other.baseService());
    }
    // Going away must not take the name from the other owner.
    QTest::qWait(100);
    QCOMPARE(viewServiceOwner(), other.baseService());

    {
        AtbAppMenuController controller;
        QTest::qWait(100);
        other.interface()->unregisterService(ViewService);
        // We were queued and take over the name.
        QTRY_VERIFY(viewServiceRegistered());
        QVERIFY(viewServiceOwner() != other.baseService());
    }
    QTRY_VERIFY(!viewServiceRegistered());
    QDBusConnection::disconnectFromBus(otherName);
}

QTEST_MAIN(AppMenuTest)

#include "appmenutest.moc"
