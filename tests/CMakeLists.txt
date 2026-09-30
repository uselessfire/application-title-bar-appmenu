# SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Test)

# The tests own D-Bus names that change the behaviour of the whole session
# (org.kde.kappmenuview), so each one runs on its own private session bus.
find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)

set(atb_test_environment
    "QT_QPA_PLATFORM=offscreen"
    "QT_QUICK_BACKEND=software"
    "QML_IMPORT_PATH=${CMAKE_BINARY_DIR}/bin"
    "ATB_APPMENU_TEST_PRIVATE_BUS=1"
)

# White box: the backend sources are compiled into the test.
add_executable(appmenutest
    appmenutest.cpp
    fakemenuexporter.cpp
    fakemenuexporter.h
    ${ATB_APPMENU_SOURCES}
)
target_include_directories(appmenutest PRIVATE ${PROJECT_SOURCE_DIR}/plugin ${PROJECT_BINARY_DIR}/plugin)
target_link_libraries(appmenutest PRIVATE
    Qt::Test
    Qt::DBus
    Qt::Widgets
    Qt::Qml
    Qt::Quick
    KF6::I18n
    atb_dbusmenuqt
)
add_test(NAME appmenutest COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:appmenutest>)
set_tests_properties(appmenutest PROPERTIES ENVIRONMENT "${atb_test_environment}")

# Black box: the built QML module is imported like the plasmoid does it.
add_executable(pluginloadtest pluginloadtest.cpp)
target_link_libraries(pluginloadtest PRIVATE Qt::Test Qt::Qml Qt::Widgets)
add_dependencies(pluginloadtest atbappmenuplugin)
add_test(NAME pluginloadtest COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:pluginloadtest>)
set_tests_properties(pluginloadtest PROPERTIES ENVIRONMENT "${atb_test_environment}")

# Black box: the menu button of the widget, loaded from the package directory.
add_executable(menubuttontest menubuttontest.cpp)
target_compile_definitions(menubuttontest PRIVATE ATB_MENU_BUTTON_SCENE="${CMAKE_CURRENT_SOURCE_DIR}/menubuttonscene.qml")
target_link_libraries(menubuttontest PRIVATE Qt::Test Qt::Qml Qt::Quick KF6::I18n KF6::I18nQml)
add_test(NAME menubuttontest COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:menubuttontest>)
set_tests_properties(menubuttontest PROPERTIES ENVIRONMENT "${atb_test_environment}")
