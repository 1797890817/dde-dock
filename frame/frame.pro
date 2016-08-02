
include(../interfaces/interfaces.pri)

QT       += core gui widgets dbus x11extras svg

TARGET          = dde-dock
DESTDIR         = $$_PRO_FILE_PWD_/../
TEMPLATE        = app
CONFIG         += c++11 link_pkgconfig

PKGCONFIG += xcb-ewmh gtk+-2.0 dtkwidget dtkbase dtkutil

SOURCES += main.cpp \
    window/mainwindow.cpp \
    xcb/xcb_misc.cpp \
    item/dockitem.cpp \
    panel/mainpanel.cpp \
    controller/dockitemcontroller.cpp \
    dbus/dbusdockentry.cpp \
    dbus/dbusdisplay.cpp \
    item/appitem.cpp \
    util/docksettings.cpp \
    dbus/dbusclientmanager.cpp \
    dbus/dbusdock.cpp \
    util/themeappicon.cpp \
    item/launcheritem.cpp \
    dbus/dbusmenumanager.cpp \
    dbus/dbusmenu.cpp \
    item/pluginsitem.cpp \
    controller/dockpluginscontroller.cpp \
    util/imagefactory.cpp \
    util/dockpopupwindow.cpp \
    dbus/dbusxmousearea.cpp \
    item/stretchitem.cpp \
    item/placeholderitem.cpp

HEADERS  += \
    window/mainwindow.h \
    xcb/xcb_misc.h \
    item/dockitem.h \
    panel/mainpanel.h \
    controller/dockitemcontroller.h \
    dbus/dbusdockentry.h \
    dbus/dbusdisplay.h \
    item/appitem.h \
    util/docksettings.h \
    dbus/dbusclientmanager.h \
    dbus/dbusdock.h \
    util/themeappicon.h \
    item/launcheritem.h \
    dbus/dbusmenumanager.h \
    dbus/dbusmenu.h \
    item/pluginsitem.h \
    controller/dockpluginscontroller.h \
    util/imagefactory.h \
    util/dockpopupwindow.h \
    dbus/dbusxmousearea.h \
    item/stretchitem.h \
    item/placeholderitem.h

dbus_service.files += com.deepin.dde.dock.service
dbus_service.path = /usr/share/dbus-1/services

target.path = $${PREFIX}/bin/
INSTALLS += target dbus_service

RESOURCES += \
    item/resources.qrc
