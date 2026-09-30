/*
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTest>

#include <memory>

namespace
{
// AppMenuButton.State.Down
constexpr int DownState = 2;
}

class MenuButtonTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void clickActivates();
    void longPressActivates();
    void dragReachesHandlerBelow();
    void pressLooksDown();
    void hoverSwitchesWhileMenuIsOpen();
    void disabledButtonIgnoresClicks();

private:
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QQuickWindow *m_window = nullptr;
    QQuickItem *m_button = nullptr;
};

void MenuButtonTest::initTestCase()
{
    // The strings of the button are not translated here anyway.
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("menubuttontest"));
}

void MenuButtonTest::init()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    // i18n() like in Plasma
    KLocalization::setupLocalizedContext(m_engine.get());
    // menubuttonscene.qml: the button above a PointHandler, like in the widget
    m_engine->load(QUrl::fromLocalFile(QStringLiteral(ATB_MENU_BUTTON_SCENE)));
    QCOMPARE(m_engine->rootObjects().size(), 1);
    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
    QVERIFY(m_window);
    m_button = m_window->property("button").value<QQuickItem *>();
    QVERIFY(m_button);
    QVERIFY(QTest::qWaitForWindowExposed(m_window));
    // Start with the pointer outside of the button.
    QTest::mouseMove(m_window, QPoint(250, 20));
}

void MenuButtonTest::cleanup()
{
    m_engine.reset();
    m_window = nullptr;
    m_button = nullptr;
}

void MenuButtonTest::clickActivates()
{
    QTest::mouseClick(m_window, Qt::LeftButton, {}, QPoint(50, 20));
    QCOMPARE(m_window->property("activations").toInt(), 1);
}

void MenuButtonTest::longPressActivates()
{
    QTest::mousePress(m_window, Qt::LeftButton, {}, QPoint(50, 20));
    QTest::qWait(qApp->styleHints()->mousePressAndHoldInterval() + 300);
    QTest::mouseRelease(m_window, Qt::LeftButton, {}, QPoint(50, 20));
    QCOMPARE(m_window->property("activations").toInt(), 1);

    // Pressed as long, but then moved: a drag of the window, no menu.
    QTest::mousePress(m_window, Qt::LeftButton, {}, QPoint(20, 20));
    QTest::qWait(qApp->styleHints()->mousePressAndHoldInterval() + 300);
    for (int x = 25; x <= 80; x += 5) {
        QTest::mouseMove(m_window, QPoint(x, 20));
    }
    QTest::mouseRelease(m_window, Qt::LeftButton, {}, QPoint(80, 20));
    QVERIFY(m_window->property("dragDistance").toReal() >= 60);
    QCOMPARE(m_window->property("activations").toInt(), 1);
}

void MenuButtonTest::dragReachesHandlerBelow()
{
    // A press that moves further than the drag threshold drags the window like the title
    // does, and opens nothing, even when it is released on the button.
    QTest::mousePress(m_window, Qt::LeftButton, {}, QPoint(20, 20));
    for (int x = 25; x <= 80; x += 5) {
        QTest::mouseMove(m_window, QPoint(x, 20));
    }
    QTest::mouseRelease(m_window, Qt::LeftButton, {}, QPoint(80, 20));
    QCOMPARE(m_window->property("dragActivations").toInt(), 1);
    QVERIFY(m_window->property("dragDistance").toReal() >= 60);
    QCOMPARE(m_window->property("activations").toInt(), 0);

    // The next click still opens the menu.
    QTest::mouseClick(m_window, Qt::LeftButton, {}, QPoint(50, 20));
    QCOMPARE(m_window->property("activations").toInt(), 1);
}

void MenuButtonTest::pressLooksDown()
{
    QVERIFY(m_button->property("menuState").toInt() != DownState);
    QTest::mousePress(m_window, Qt::LeftButton, {}, QPoint(50, 20));
    QCOMPARE(m_button->property("menuState").toInt(), DownState);
    QTest::mouseRelease(m_window, Qt::LeftButton, {}, QPoint(50, 20));
    QVERIFY(m_button->property("menuState").toInt() != DownState);

    // And while its menu is open.
    m_button->setProperty("down", true);
    QCOMPARE(m_button->property("menuState").toInt(), DownState);
}

void MenuButtonTest::hoverSwitchesWhileMenuIsOpen()
{
    // Passing over the entry with no menu open does nothing.
    QTest::mouseMove(m_window, QPoint(50, 20));
    QTest::mouseMove(m_window, QPoint(250, 20));
    QCOMPARE(m_window->property("switches").toInt(), 0);

    m_button->setProperty("menuIsOpen", true);
    QTest::mouseMove(m_window, QPoint(50, 20));
    QCOMPARE(m_window->property("switches").toInt(), 1);
    QCOMPARE(m_window->property("activations").toInt(), 0);
}

void MenuButtonTest::disabledButtonIgnoresClicks()
{
    m_button->setEnabled(false);
    QTest::mouseClick(m_window, Qt::LeftButton, {}, QPoint(50, 20));
    QCOMPARE(m_window->property("activations").toInt(), 0);
}

QTEST_MAIN(MenuButtonTest)

#include "menubuttontest.moc"
