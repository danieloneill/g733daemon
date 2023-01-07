#include "headsetdbusservice.h"

#include <QCoreApplication>
#include <QTimer>

HeadsetDBusService::HeadsetDBusService(QObject *obj, HeadsetHID *h)
    : QDBusAbstractAdaptor{obj},
      m_hid(h)
{
    connect( m_hid, &HeadsetHID::chargingChanged, this, &HeadsetDBusService::chargingChanged );
    connect( m_hid, &HeadsetHID::onlineChanged, this, &HeadsetDBusService::onlineChanged );
    connect( m_hid, &HeadsetHID::voltageChanged, this, &HeadsetDBusService::voltageChanged );
    connect( m_hid, &HeadsetHID::socChanged, this, &HeadsetDBusService::socChanged );
    connect( m_hid, &HeadsetHID::lightingChanged, this, &HeadsetDBusService::lightingChanged );
    connect( m_hid, &HeadsetHID::buttonPressed, this, &HeadsetDBusService::buttonPressed );
}

bool HeadsetDBusService::online()
{
    if( m_hid )
        return m_hid->online();

    return false;
}

bool HeadsetDBusService::charging()
{
    if( m_hid )
        return m_hid->charging();

    return false;
}

bool HeadsetDBusService::lighting()
{
    if( m_hid )
        return m_hid->lighting();

    return false;
}

int HeadsetDBusService::voltage()
{
    if( m_hid )
        return m_hid->voltage();

    return -100;
}

int HeadsetDBusService::soc()
{
    if( m_hid )
        return m_hid->soc();

    return -1;
}

void HeadsetDBusService::setLighting(bool onoff)
{
    if( !m_hid )
        return;

    m_hid->enableLighting(onoff);
}

void HeadsetDBusService::quit()
{
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}
