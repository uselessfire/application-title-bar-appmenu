/*
 * SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid
import org.kde.taskmanager as TaskManager
import "utils.js" as Utils
import "config/effect/"
import "config/effect/effect.js" as EffectUtils

PlasmoidItem {
    id: root

    property TaskManager.TasksModel tasksModel
    property real widgetHeight: (vertical ? width : height)
    property real elementHeight: widgetHeight - plasmoid.configuration.widgetMargins * 2
    property real buttonMargins: plasmoid.configuration.widgetButtonsMargins
    property real buttonHeight: elementHeight
    property real buttonWidth: (plasmoid.configuration.widgetButtonsAspectRatio) / 100 * (buttonHeight - buttonMargins * 2)
    property var widgetAlignment: plasmoid.configuration.widgetHorizontalAlignment | plasmoid.configuration.widgetVerticalAlignment
    property KWinConfig kWinConfig
    property bool widgetHovered: widgetHoverHandler.hovered
    property bool vertical: plasmoid.formFactor === PlasmaCore.Types.Vertical
    property bool leftEdgeLocation: plasmoid.location === PlasmaCore.Types.LeftEdge
    property bool hideWidget: !tasksModel.hasActiveWindow && plasmoid.configuration.widgetElementsDisabledMode === WidgetElement.DisabledMode.Hide
    property bool editMode: Plasmoid.containment.corona?.editMode ?? false

    // Application menu, see AppMenuBar.qml and AppMenuBackend.qml
    readonly property string appMenuElementName: "appMenuBar"
    readonly property bool appMenuConfigured: plasmoid.configuration.widgetElements.includes(appMenuElementName) || (plasmoid.configuration.overrideElementsMaximized && plasmoid.configuration.widgetElementsMaximized.includes(appMenuElementName))
    readonly property bool appMenuRevealOnHover: plasmoid.configuration.appMenuRevealOnHover && !vertical
    // The visible "Application Menu Bar" element, if any
    property AppMenuBar appMenuBar: null
    // The visible window title element, if any, and its width while it is fully shown
    property var appMenuTitle: null
    readonly property real appMenuTitleWidth: appMenuTitle ? appMenuTitle.expandedWidth : 0
    readonly property var appMenuModel: appMenuBackend.item ? appMenuBackend.item.model : null
    readonly property var appMenuController: appMenuBackend.item ? appMenuBackend.item.controller : null
    readonly property int appMenuCurrentIndex: appMenuController ? appMenuController.currentIndex : -1
    readonly property bool appMenuOpen: appMenuCurrentIndex !== -1
    readonly property bool appMenuHasMenu: appMenuBar !== null && appMenuBar.naturalWidth > 0
    // Follows appMenuHasMenu, but ignores short gaps while a menu is being reloaded
    property bool appMenuHasMenuStable: false
    readonly property bool appMenuAltPressed: appMenuAltKeyState.item ? appMenuAltKeyState.item.pressed : false
    // The pointer rested on the widget: show the menu in place of the title until it leaves
    property bool appMenuLatched: false
    // A menu the application asked to open while the menu bar was still collapsed
    property int appMenuPendingIndex: -1
    property bool appMenuWidgetRemoved: false
    readonly property bool appMenuRevealed: appMenuHasMenuStable && (!appMenuRevealOnHover || appMenuLatched || appMenuOpen || appMenuPendingIndex >= 0)
    // 0: the title is shown, 1: the menu is shown. Both widths follow this single value.
    property real appMenuProgress: appMenuRevealed ? 1 : 0
    readonly property real appMenuTitleCollapse: appMenuRevealOnHover ? appMenuProgress : 0
    // The pointer is over the menu bar, or one of its menus is open
    readonly property bool appMenuActive: appMenuOpen || (appMenuBar !== null && appMenuBar.hovered)

    signal invokeKWinShortcut(string shortcut)
    signal widgetElementsLayoutUpdated

    function appMenuPointerMoved() {
        if (appMenuRevealOnHover && appMenuHasMenuStable && !appMenuLatched && !appMenuRevealTimer.running) {
            appMenuRevealTimer.start();
        }
    }

    function appMenuPointerLeft() {
        appMenuRevealTimer.stop();
    }

    // Opens the menu at index, revealing the menu bar first if necessary.
    function appMenuActivate(index) {
        if (!appMenuHasMenuStable || !appMenuController || index < 0) {
            return;
        }
        if (appMenuProgress < 1) {
            appMenuPendingIndex = index;
            return;
        }
        appMenuTrigger(index);
    }

    function appMenuTrigger(index) {
        const button = appMenuBar ? appMenuBar.buttonAt(index) : null;
        if (appMenuController && button && button.visible && button.width > 0) {
            appMenuController.trigger(button, index);
        }
    }

    function appMenuFlushPending() {
        const index = appMenuPendingIndex;
        appMenuTrigger(index);
        appMenuPendingIndex = -1;
        if (!appMenuOpen && !widgetHovered) {
            appMenuCollapseTimer.restart();
        }
    }

    Plasmoid.constraintHints: Plasmoid.CanFillArea
    // NeedsAttention keeps an auto-hiding panel visible while a menu is open.
    Plasmoid.status: hideWidget ? PlasmaCore.Types.HiddenStatus : (appMenuOpen ? PlasmaCore.Types.NeedsAttentionStatus : PlasmaCore.Types.ActiveStatus)
    Layout.fillWidth: !vertical && plasmoid.configuration.widgetFillWidth
    Layout.fillHeight: vertical && plasmoid.configuration.widgetFillWidth
    preferredRepresentation: fullRepresentation
    onInvokeKWinShortcut: function (shortcut) {
        if (tasksModel.hasActiveWindow)
            tasksModel.activeWindow.actionCall(ActiveWindow.Action.Activate);

        kWinConfig.invokeKWinShortcut(shortcut);
    }
    onWidgetHoveredChanged: {
        if (widgetHovered) {
            appMenuCollapseTimer.stop();
        } else {
            appMenuCollapseTimer.restart();
        }
    }
    onAppMenuOpenChanged: {
        if (!appMenuOpen && !widgetHovered) {
            appMenuCollapseTimer.restart();
        }
    }
    onAppMenuRevealOnHoverChanged: {
        appMenuLatched = false;
        appMenuRevealTimer.stop();
    }
    onAppMenuHasMenuChanged: {
        if (appMenuHasMenu) {
            appMenuHasMenuTimer.stop();
            appMenuHasMenuStable = true;
        } else {
            appMenuHasMenuTimer.restart();
        }
    }
    onAppMenuHasMenuStableChanged: {
        // A menu requested for a window that has no menu any more.
        if (!appMenuHasMenuStable) {
            appMenuPendingIndex = -1;
        }
    }
    onAppMenuProgressChanged: {
        if (appMenuProgress === 1 && appMenuPendingIndex >= 0) {
            // Let the layout settle, the menu is positioned at its button.
            appMenuPendingTimer.restart();
        }
    }

    Behavior on appMenuProgress {
        enabled: root.appMenuRevealOnHover

        NumberAnimation {
            duration: Kirigami.Units.longDuration
            easing.type: Easing.InOutQuad
        }
    }

    Timer {
        id: appMenuRevealTimer

        // Only reveal when the pointer rests on the widget, not when it passes by.
        interval: 150
        onTriggered: {
            if (root.appMenuRevealOnHover && root.appMenuHasMenuStable) {
                root.appMenuLatched = true;
                if (!root.widgetHovered) {
                    appMenuCollapseTimer.restart();
                }
            }
        }
    }

    Timer {
        id: appMenuCollapseTimer

        interval: 400
        onTriggered: {
            if (!root.widgetHovered && !root.appMenuOpen && root.appMenuPendingIndex < 0) {
                root.appMenuLatched = false;
            }
        }
    }

    Timer {
        id: appMenuHasMenuTimer

        interval: 150
        onTriggered: root.appMenuHasMenuStable = root.appMenuHasMenu
    }

    Timer {
        id: appMenuPendingTimer

        interval: 50
        onTriggered: root.appMenuFlushPending()
    }

    Loader {
        id: appMenuBackend

        // Needs the compiled module, see AppMenuBackend.qml. While the widget is removed
        // but can still be restored, applications get their own menu bars back.
        active: root.appMenuConfigured && !root.vertical && !root.appMenuWidgetRemoved
        source: "AppMenuBackend.qml"
        onLoaded: {
            item.tasksModel = root.tasksModel;
            item.widget = root;
        }
    }

    Loader {
        id: appMenuAltKeyState

        active: appMenuBackend.status === Loader.Ready
        source: "AppMenuAltKeyState.qml"
    }

    Connections {
        target: Plasmoid

        // The global shortcut of the widget opens the first menu.
        function onActivated() {
            if (root.appMenuBar) {
                root.appMenuActivate(root.appMenuBar.firstButtonIndex());
            }
        }

        function onDestroyedChanged(destroyed) {
            root.appMenuWidgetRemoved = destroyed;
        }
    }

    ContextualActions {}

    Component {
        id: widgetElementLoaderDelegate

        Loader {
            id: widgetElementLoader

            required property int index
            required property var modelData
            property bool repeaterVisible: false

            onLoaded: function () {
                Utils.copyLayoutConstraint(item, widgetElementLoader);
                widgetElementLoader.Layout.preferredWidthChanged.connect(root.widgetElementsLayoutUpdated);
                item.modelData = modelData;
            }
            sourceComponent: {
                switch (modelData.type) {
                case WidgetElement.Type.WindowControlButton:
                    return windowControlButton;
                case WidgetElement.Type.WindowTitle:
                    return windowTitle;
                case WidgetElement.Type.WindowIcon:
                    return windowIcon;
                case WidgetElement.Type.Spacer:
                    return spacerIcon;
                case WidgetElement.Type.AppMenuBar:
                    return appMenuBarElement;
                }
            }

            Binding {
                when: status === Loader.Ready
                widgetElementLoader.visible: repeaterVisible && (plasmoid.configuration.widgetElementsDisabledMode === WidgetElement.DisabledMode.Hide ? item.enabled : true)
            }

            Binding {
                function itemVisible(itemEnabled) {
                    switch (plasmoid.configuration.widgetElementsDisabledMode) {
                    case WidgetElement.DisabledMode.Hide:
                        return itemEnabled;
                    case WidgetElement.DisabledMode.HideKeepSpace:
                        return itemEnabled;
                    default:
                        return true;
                    }
                }

                when: status === Loader.Ready
                target: item
                property: "visible"
                value: itemVisible(item.enabled)
            }
        }
    }

    Component {
        id: windowControlButton

        WindowControlButton {
            id: windowControlButton

            property var modelData
            Layout.alignment: root.widgetAlignment
            Layout.preferredWidth: root.buttonWidth
            Layout.preferredHeight: root.buttonHeight
            verticalPadding: root.buttonMargins
            buttonType: modelData.windowControlButtonType
            themeName: plasmoid.configuration.widgetButtonsAuroraeTheme
            iconTheme: plasmoid.configuration.widgetButtonsIconsTheme
            animationDuration: plasmoid.configuration.widgetButtonsAnimation
            onActionCall: action => {
                return tasksModel.activeWindow.actionCall(action);
            }
            enabled: tasksModel.hasActiveWindow && tasksModel.activeWindow.actionSupported(action) && (!plasmoid.configuration.disableButtonsForNotHoveredWidget || root.widgetHovered || root.appMenuOpen)
            checked: tasksModel.hasActiveWindow && tasksModel.activeWindow.buttonChecked(modelData.windowControlButtonType)
            active: tasksModel.hasActiveWindow && tasksModel.activeWindow.active
        }
    }

    Component {
        id: windowIcon

        Kirigami.Icon {
            property var modelData

            height: root.elementHeight
            width: height
            Layout.alignment: root.widgetAlignment
            Layout.preferredWidth: width
            source: tasksModel.activeWindow.icon || "window"
            enabled: tasksModel.hasActiveWindow && !!tasksModel.activeWindow.icon
        }
    }

    Component {
        id: spacerIcon

        Rectangle {
            property var modelData

            height: root.elementHeight
            width: height / 3
            Layout.alignment: root.widgetAlignment
            Layout.preferredWidth: width
            color: "transparent"
            enabled: tasksModel.hasActiveWindow
        }
    }

    Component {
        id: appMenuBarElement

        AppMenuBar {
            widget: root
        }
    }

    Component {
        id: windowTitle

        PlasmaComponents.Label {
            id: windowTitleLabel

            readonly property var horizontalAlignmentValues: [Text.AlignLeft, Text.AlignRight, Text.AlignHCenter, Text.AlignJustify]
            readonly property var verticalAlignmentValues: [Text.AlignTop, Text.AlignBottom, Text.AlignVCenter]

            property var modelData
            property bool empty: text === undefined || text === ""
            property bool hideEmpty: empty && plasmoid.configuration.windowTitleHideEmpty
            property int windowTitleSource: plasmoid.configuration.overrideElementsMaximized && tasksModel.activeWindow.maximized ? plasmoid.configuration.windowTitleSourceMaximized : plasmoid.configuration.windowTitleSource
            property var titleTextReplacements: []
            readonly property real naturalWidth: textMetrics.advanceWidth + leftPadding + rightPadding + 1 // Magic number
            // The width of the title while it is fully shown. It differs from the natural
            // width when the widget fills free space.
            property real expandedWidth: naturalWidth

            // While the application menu takes its place, the title shrinks from its full
            // width and fades out.
            Layout.minimumWidth: plasmoid.configuration.windowTitleMinimumWidth * (1 - root.appMenuTitleCollapse)
            Layout.maximumWidth: {
                if (hideEmpty) {
                    return 0;
                }
                if (root.appMenuTitleCollapse > 0) {
                    return expandedWidth * (1 - root.appMenuTitleCollapse);
                }
                return plasmoid.configuration.windowTitleMaximumWidth;
            }
            onWidthChanged: {
                if (root.appMenuTitleCollapse === 0) {
                    expandedWidth = width;
                }
            }
            onNaturalWidthChanged: {
                // The text changed while the menu takes the place of the title. Unless the
                // title fills the free space, its full width follows from the text.
                if (root.appMenuTitleCollapse > 0 && !plasmoid.configuration.widgetFillWidth) {
                    const maximum = plasmoid.configuration.windowTitleMaximumWidth;
                    const width = maximum >= 0 ? Math.min(naturalWidth, maximum) : naturalWidth;
                    expandedWidth = Math.max(plasmoid.configuration.windowTitleMinimumWidth, width);
                }
            }
            onVisibleChanged: updateAppMenuRegistration()
            Component.onDestruction: {
                if (root.appMenuTitle === windowTitleLabel) {
                    root.appMenuTitle = null;
                }
            }
            Layout.alignment: root.widgetAlignment
            Layout.fillWidth: plasmoid.configuration.widgetFillWidth
            Layout.fillHeight: true
            Layout.preferredWidth: naturalWidth
            opacity: 1 - root.appMenuTitleCollapse
            text: titleText(windowTitleSource) || plasmoid.configuration.windowTitleUndefined
            font.pointSize: plasmoid.configuration.windowTitleFontSize
            font.bold: plasmoid.configuration.windowTitleFontBold
            fontSizeMode: plasmoid.configuration.windowTitleFontSizeMode
            maximumLineCount: 1
            elide: Text.ElideRight
            wrapMode: Text.WrapAnywhere
            enabled: tasksModel.hasActiveWindow
            horizontalAlignment: horizontalAlignmentValues[plasmoid.configuration.windowTitleHorizontalAlignment]
            verticalAlignment: verticalAlignmentValues[plasmoid.configuration.windowTitleVerticalAlignment]

            bottomPadding: !hideEmpty ? plasmoid.configuration.windowTitleMarginsBottom : 0
            leftPadding: !hideEmpty ? plasmoid.configuration.windowTitleMarginsLeft : 0
            rightPadding: !hideEmpty ? plasmoid.configuration.windowTitleMarginsRight : 0
            topPadding: !hideEmpty ? plasmoid.configuration.windowTitleMarginsTop : 0

            Accessible.role: Accessible.TitleBar
            Accessible.name: text

            TextMetrics {
                id: textMetrics
                font: windowTitleLabel.font
                text: windowTitleLabel.text
            }

            Connections {
                target: plasmoid.configuration

                function onTitleReplacementsTypesChanged() {
                    updateTitleTextReplacements();
                }

                function onTitleReplacementsPatternsChanged() {
                    updateTitleTextReplacements();
                }

                function onTitleReplacementsTemplatesChanged() {
                    updateTitleTextReplacements();
                }
            }

            Component.onCompleted: {
                updateTitleTextReplacements();
                updateAppMenuRegistration();
            }

            // The widget may contain two titles, the second one for maximized windows.
            function updateAppMenuRegistration() {
                if (visible) {
                    root.appMenuTitle = windowTitleLabel;
                } else if (root.appMenuTitle === windowTitleLabel) {
                    root.appMenuTitle = null;
                }
            }

            function titleText(windowTitleSource) {
                let titleTextResult = "";
                switch (windowTitleSource) {
                case 0:
                    titleTextResult = tasksModel.activeWindow.appName;
                    break;
                case 1:
                    titleTextResult = tasksModel.activeWindow.decoration;
                    break;
                case 2:
                    titleTextResult = tasksModel.activeWindow.genericAppName;
                    break;
                case 3:
                    titleTextResult = plasmoid.configuration.windowTitleUndefined;
                    break;
                }
                if (titleTextResult) {
                    titleTextResult = Utils.Replacement.applyReplacementList(titleTextResult, titleTextReplacements);
                }
                return titleTextResult;
            }

            function updateTitleTextReplacements() {
                Qt.callLater(_updateTitleTextReplacements);
            }

            function _updateTitleTextReplacements() {
                titleTextReplacements = Utils.Replacement.createReplacementList(plasmoid.configuration.titleReplacementsTypes, plasmoid.configuration.titleReplacementsPatterns, plasmoid.configuration.titleReplacementsTemplates);
            }
        }
    }

    kWinConfig: KWinConfig {
        Component.onCompleted: updateKWinShortcutNames()
    }

    tasksModel: ActiveTasksModel {
        id: tasksModel
    }

    HoverHandler {
        id: widgetHoverHandler
        enabled: plasmoid.configuration.disableButtonsForNotHoveredWidget || root.appMenuRevealOnHover
    }

    fullRepresentation: Item {
        id: representationProxy

        Layout.fillWidth: root.vertical ? null : plasmoid.configuration.widgetFillWidth
        Layout.fillHeight: root.vertical ? plasmoid.configuration.widgetFillWidth : null

        Layout.minimumWidth: root.vertical ? widgetRow.Layout.minimumHeight : widgetRow.Layout.minimumWidth
        Layout.minimumHeight: root.vertical ? widgetRow.Layout.minimumWidth : widgetRow.Layout.minimumHeight

        Layout.maximumWidth: root.vertical ? widgetRow.Layout.maximumHeight : widgetRow.Layout.maximumWidth
        Layout.maximumHeight: root.vertical ? widgetRow.Layout.maximumWidth : widgetRow.Layout.maximumHeight

        Layout.preferredWidth: root.vertical ? widgetRow.Layout.preferredHeight : widgetRow.Layout.preferredWidth
        Layout.preferredHeight: root.vertical ? widgetRow.Layout.preferredWidth : widgetRow.Layout.preferredHeight

        MouseHandlers {
            // An open menu has the pointer. Over the menu bar, clicks and the wheel belong
            // to the menus, while a press there can still drag the window: menus open on click.
            dragEnabled: !root.appMenuOpen
            clicksEnabled: !root.appMenuActive
            Component.onCompleted: {
                invokeKWinShortcut.connect(root.invokeKWinShortcut);
            }
        }

        WidgetToolTip {
            id: widgetToolTip

            anchors.fill: parent
            tasksModel: root.tasksModel
            suppressed: root.appMenuActive || (root.appMenuRevealOnHover && root.appMenuRevealed)

            // Reveals the application menu in place of the title when the pointer moves over the
            // widget, except over the window buttons: they may move when the menu replaces the
            // title, and must not do that under the pointer. The tooltip area fills the widget
            // and accepts the hover events, which keeps them from the handlers of the items
            // below it, but not from its own handlers.
            HoverHandler {
                property point lastPosition: Qt.point(-1, -1)

                function isOverWindowButton(position) {
                    const rowPosition = widgetRow.mapFromItem(widgetToolTip, position);
                    const element = widgetRow.childAt(rowPosition.x, rowPosition.y) as Loader;
                    return element !== null && element.item instanceof WindowControlButton;
                }

                enabled: root.appMenuRevealOnHover
                onPointChanged: {
                    // React to real pointer movement only: when the layout moves the elements
                    // under a resting pointer, Qt reports a hover as well.
                    const scenePosition = point.scenePosition;
                    const moved = scenePosition.x !== lastPosition.x || scenePosition.y !== lastPosition.y;
                    lastPosition = scenePosition;
                    if (!hovered || !moved) {
                        return;
                    }
                    if (isOverWindowButton(point.position)) {
                        root.appMenuPointerLeft();
                    } else {
                        root.appMenuPointerMoved();
                    }
                }
                onHoveredChanged: {
                    if (!hovered) {
                        // Coming back is a movement, even to the position where the pointer left.
                        lastPosition = Qt.point(-1, -1);
                        root.appMenuPointerLeft();
                    }
                }
            }
        }

        RowLayout {
            id: widgetRow

            spacing: plasmoid.configuration.widgetSpacing
            anchors.left: parent.left
            anchors.verticalCenter: root.vertical ? undefined : parent.verticalCenter
            anchors.horizontalCenter: root.vertical ? parent.horizontalCenter : undefined
            width: root.vertical ? representationProxy.height : representationProxy.width
            height: root.vertical ? representationProxy.width : representationProxy.height

            Accessible.role: Accessible.Grouping
            Accessible.name: i18n("Application title bar")

            transform: [
                Rotation {
                    angle: root.vertical ? 90 : 0
                    origin.x: widgetHeight / 2 - plasmoid.configuration.widgetMargins / 2 // IDK why
                    origin.y: widgetHeight / 2 - plasmoid.configuration.widgetMargins / 2
                },
                Rotation {
                    angle: root.leftEdgeLocation ? 180 : 0
                    origin.x: width / 2
                    origin.y: height / 2
                }
            ]

            PlasmaComponents.Label {
                id: editModePlaceholder
                text: Plasmoid.metaData.name
                visible: editMode
            }

            Repeater {
                id: widgetElementsRepeater
                property var elements: plasmoid.configuration.widgetElements

                onElementsChanged: function () {
                    let array = [];
                    for (var i = 0; i < elements.length; i++) {
                        array.push(Utils.widgetElementModelFromName(elements[i]));
                    }
                    model = array;
                }
                model: []
                delegate: widgetElementLoaderDelegate
                visible: !plasmoid.configuration.overrideElementsMaximized || !tasksModel.activeWindow.maximized
                onItemAdded: function (index, item) {
                    item.repeaterVisible = Qt.binding(function () {
                        return visible;
                    });
                }
            }

            Repeater {
                id: widgetElementsMaximizedRepeater
                property var elements: plasmoid.configuration.overrideElementsMaximized ? plasmoid.configuration.widgetElementsMaximized : []

                onElementsChanged: function () {
                    let array = [];
                    for (var i = 0; i < elements.length; i++) {
                        array.push(Utils.widgetElementModelFromName(elements[i]));
                    }
                    model = array;
                }
                model: []
                delegate: widgetElementLoaderDelegate
                visible: !widgetElementsRepeater.visible
                onItemAdded: function (index, item) {
                    item.repeaterVisible = Qt.binding(function () {
                        return visible;
                    });
                }
            }

            Connections {
                target: root

                function onWidgetElementsLayoutUpdated() {
                    var preferredWidth = plasmoid.configuration.widgetFillWidth ? widgetRow.calculatePreferredWidth() : -1;
                    widgetRow.Layout.preferredWidth = preferredWidth;
                }
            }

            function calculatePreferredWidth() {
                var repeater = widgetElementsRepeater.visible ? widgetElementsRepeater : widgetElementsMaximizedRepeater;
                var preferredWidth = (repeater.count - 1) * widgetRow.spacing;
                for (var i = 0; i < repeater.count; i++) {
                    var item = repeater.itemAt(i);
                    preferredWidth += Utils.calculateItemPreferredWidth(item);
                }
                if (preferredWidth < widgetRow.Layout.minimumWidth) {
                    return widgetRow.Layout.minimumWidth;
                } else if (preferredWidth > widgetRow.Layout.maximumWidth) {
                    return widgetRow.Layout.maximumWidth;
                } else {
                    return preferredWidth;
                }
            }
        }

        WidgetEffectsRepeater {
            id: effectsRepeater
        }

        Component.onCompleted: effectsRepeater.updateEffectRules()

        Connections {
            target: root.tasksModel
            function onActiveWindowUpdated() {
                effectsRepeater.updateEffectsState();
            }
        }

        Connections {
            target: plasmoid.configuration
            function onEffectRulesChanged() {
                effectsRepeater.updateEffectRules();
            }
            function onEffectsChanged() {
                effectsRepeater.updateEffectRules();
            }
        }
    }
}
