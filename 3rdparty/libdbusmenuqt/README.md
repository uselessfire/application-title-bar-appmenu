# Vendored libdbusmenuqt

This directory contains a copy of the DBusMenu importer from
[plasma-workspace](https://invent.kde.org/plasma/plasma-workspace/-/tree/v6.7.5/libdbusmenuqt)
v6.7.5, which itself is a Qt 6 port of the import path of Canonical's libdbusmenu-qt.
The code is licensed under `LGPL-2.0-or-later` (see `LICENSES/LGPL-2.0-or-later.txt`);
`com.canonical.dbusmenu.xml` carries no license header and comes from the same project.

Local modifications:

- All types, the importer and the helper functions live in the `AtbDBusMenu`
  namespace. plasmashell may load other plugins that statically link their own copy
  of this library (for example the stock Global Menu applet); the namespace keeps the
  `QMetaType` names of the D-Bus types unique, so the copies cannot interfere.
  The type annotations in `com.canonical.dbusmenu.xml` use the fully qualified names,
  because QtDBus looks some parameter types up by name.
- The logging category is `com.github.uselessfire.applicationtitlebar.appmenu.dbusmenu`.
- `CMakeLists.txt` was rewritten for this project, the manual test application
  and the outdated `README` were not copied.
