#ifndef HEADSETDBUSSERVICE_H
#define HEADSETDBUSSERVICE_H

#include <QObject>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusVariant>

#include "headsethid.h"

#define SERVICE_NAME "org.logitech.Headset.Power"

class HeadsetDBusService : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.logitech.Headset.Power.Service")

    HeadsetHID *m_hid;

    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY chargingChanged)
    Q_PROPERTY(int voltage READ voltage NOTIFY voltageChanged)
    Q_PROPERTY(int soc READ soc NOTIFY socChanged)
    Q_PROPERTY(bool lighting READ lighting WRITE setLighting NOTIFY lightingChanged)

public:
    explicit HeadsetDBusService(QObject *obj, HeadsetHID *h);

public slots:
    bool online();
    bool charging();
    bool lighting();
    int voltage();
    int soc();
    Q_NOREPLY void setLighting(bool onoff);
    Q_NOREPLY void quit();

signals:
    void chargingChanged(bool onoff);
    void onlineChanged(bool onoff);
    void voltageChanged(qint32 voltage);
    void socChanged(qint32 soc);
    void lightingChanged(bool onoff);
    void buttonPressed(int index, bool pressed);
    void aboutToQuit();
};

#endif // HEADSETDBUSSERVICE_H
