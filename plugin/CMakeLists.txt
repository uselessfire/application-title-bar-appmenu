# SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# QML module with the application menu backend. The plasmoid imports it only through
# a Loader, so the widget keeps working when the module is not installed.
ecm_add_qml_module(atbappmenuplugin
    URI com.github.uselessfire.applicationtitlebar.appmenu
    VERSION 1.0
    GENERATE_PLUGIN_SOURCE
    DEPENDENCIES QtCore QtQuick
)

set(atbappmenu_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/appmenumodel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/appmenumodel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/appmenucontroller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/appmenucontroller.h
)
ecm_qt_declare_logging_category(atbappmenu_SRCS
    HEADER appmenudebug.h
    IDENTIFIER ATB_APPMENU
    CATEGORY_NAME com.github.uselessfire.applicationtitlebar.appmenu
    DEFAULT_SEVERITY Info
    DESCRIPTION "Application Title Bar AppMenu"
    EXPORT ATB_APPMENU
)
# The tests compile the same sources: the plugin itself hides its symbols.
set(ATB_APPMENU_SOURCES ${atbappmenu_SRCS} PARENT_SCOPE)

target_sources(atbappmenuplugin PRIVATE ${atbappmenu_SRCS})
target_link_libraries(atbappmenuplugin PRIVATE
    Qt::Core
    Qt::Gui
    Qt::DBus
    Qt::Widgets
    Qt::Qml
    Qt::Quick
    KF6::I18n
    atb_dbusmenuqt
)

ecm_finalize_qml_module(atbappmenuplugin)
