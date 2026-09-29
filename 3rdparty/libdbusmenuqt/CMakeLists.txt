# SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build file for the vendored copy of libdbusmenuqt, adapted from plasma-workspace.

set(dbusmenuqt_SRCS
    dbusmenuimporter.cpp
    dbusmenuimporter.h
    dbusmenushortcut_p.cpp
    dbusmenushortcut_p.h
    dbusmenutypes_p.cpp
    dbusmenutypes_p.h
    utils.cpp
    utils_p.h
)

ecm_qt_declare_logging_category(dbusmenuqt_SRCS
    HEADER debug.h
    IDENTIFIER DBUSMENUQT
    CATEGORY_NAME com.github.uselessfire.applicationtitlebar.appmenu.dbusmenu
    DEFAULT_SEVERITY Info
    DESCRIPTION "Application Title Bar AppMenu (vendored libdbusmenuqt)"
    EXPORT ATB_APPMENU
)

set_source_files_properties(com.canonical.dbusmenu.xml PROPERTIES
    NO_NAMESPACE true
    INCLUDE "dbusmenutypes_p.h"
    CLASSNAME DBusMenuInterface
)
qt_add_dbus_interface(dbusmenuqt_SRCS com.canonical.dbusmenu.xml dbusmenu_interface)

add_library(atb_dbusmenuqt STATIC ${dbusmenuqt_SRCS})
target_include_directories(atb_dbusmenuqt
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(atb_dbusmenuqt PUBLIC Qt::DBus Qt::Widgets)
# The library is linked into a QML plugin that is loaded into plasmashell next to
# other copies of libdbusmenuqt, so none of its symbols may leak out of the plugin.
set_target_properties(atb_dbusmenuqt PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
