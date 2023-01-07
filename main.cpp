#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>

#include "headsetdbusservice.h"
#include "headsethid.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    HeadsetHID *h = new HeadsetHID();
    h->open();
    //hs->setHIDInterface(h);

    QObject obj;
    HeadsetDBusService *hs = new HeadsetDBusService(&obj, h);
    QObject::connect(&a, &QCoreApplication::aboutToQuit, hs, &HeadsetDBusService::aboutToQuit);
    QDBusConnection::sessionBus().registerObject("/", &obj);

    if (!QDBusConnection::sessionBus().registerService(SERVICE_NAME)) {
        fprintf(stderr, "%s\n",
                qPrintable(QDBusConnection::sessionBus().lastError().message()));
        exit(1);
    }

    return a.exec();
}
