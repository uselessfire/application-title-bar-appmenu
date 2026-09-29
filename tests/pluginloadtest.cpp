/*
    SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTest>

#include <memory>

// Loads the built QML module the same way the plasmoid does.
class PluginLoadTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadsTypes()
    {
        if (!qEnvironmentVariableIsSet("ATB_APPMENU_TEST_PRIVATE_BUS")) {
            QSKIP("Run through ctest, the test needs a private session bus (dbus-run-session)");
        }

        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
import QtQml
import com.github.uselessfire.applicationtitlebar.appmenu as AppMenu

QtObject {
    property QtObject model: AppMenu.AppMenuModel {
        searchEnabled: true
    }
    property QtObject controller: AppMenu.AppMenuController {
        popupEdge: Qt.BottomEdge
    }
    Component.onCompleted: controller.model = model
}
)",
                          QUrl(QStringLiteral("file:///pluginloadtest.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        std::unique_ptr<QObject> object(component.create());
        QVERIFY(object);
        auto *model = object->property("model").value<QObject *>();
        auto *controller = object->property("controller").value<QObject *>();
        QVERIFY(model);
        QVERIFY(controller);
        QCOMPARE(model->property("count").toInt(), 0);
        QCOMPARE(model->property("menuAvailable").toBool(), false);
        QCOMPARE(controller->property("currentIndex").toInt(), -1);
        QCOMPARE(controller->property("popupEdge").toInt(), int(Qt::BottomEdge));
        QCOMPARE(controller->property("model").value<QObject *>(), model);
    }
};

QTEST_MAIN(PluginLoadTest)

#include "pluginloadtest.moc"
