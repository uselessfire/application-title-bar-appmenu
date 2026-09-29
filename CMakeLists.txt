# SPDX-FileCopyrightText: 2026 uselessfire <uselessfire@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

# The plasmoid id and version are defined once, in the package metadata.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package/metadata.json" ATB_METADATA)
string(JSON ATB_PLASMOID_ID GET "${ATB_METADATA}" KPlugin Id)
string(JSON ATB_VERSION GET "${ATB_METADATA}" KPlugin Version)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/package/metadata.json")

project(application-title-bar-appmenu VERSION ${ATB_VERSION} LANGUAGES CXX)

set(QT_MIN_VERSION "6.8.0")
set(KF6_MIN_VERSION "6.14.0")
set(PLASMA_MIN_VERSION "6.4.0")
set(KDE_COMPILERSETTINGS_LEVEL "6.13")

include(FeatureSummary)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED URL "https://invent.kde.org/frameworks/extra-cmake-modules")
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(ECMQmlModule)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui DBus Widgets Qml Quick)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS I18n)
find_package(Plasma ${PLASMA_MIN_VERSION} REQUIRED)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
qt_policy(SET QTP0005 NEW)

add_subdirectory(3rdparty/libdbusmenuqt)
add_subdirectory(plugin)

# Lets kdebugsettings enable the debug output of the module.
ecm_qt_install_logging_categories(
    EXPORT ATB_APPMENU
    FILE application-title-bar-appmenu.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

plasma_install_package(package ${ATB_PLASMOID_ID})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
