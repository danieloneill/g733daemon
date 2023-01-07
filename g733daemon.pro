QT -= gui
QT += dbus

CONFIG += c++17 console
CONFIG -= app_bundle
CONFIG += link_pkgconfig

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

PKGCONFIG += hidapi-hidraw

SOURCES += \
        headsetdbusservice.cpp \
        headsethid.cpp \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    headsetdbusservice.h \
    headsethid.h

DISTFILES += \
    maps/charging_ascending.csv \
    maps/charging_descending.csv \
    maps/discharging.csv

resources.files = $${DISTFILES}
resources.prefix = /
RESOURCES += resources
