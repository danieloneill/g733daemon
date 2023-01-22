#include "headsethid.h"

#include <QDebug>
#include <QFile>

#define VENDOR_LOGITECH     0x046d
#define ID_LOGITECH_G733    0x0ab5

#ifndef HIDPP_LONG_MESSAGE
# define HIDPP_LONG_MESSAGE 0x11
# define HIDPP_LONG_MESSAGE_LENGTH 20
# define HIDPP_DEVICE_RECEIVER 0xff
#endif

HeadsetHID::HeadsetHID(QObject *parent)
    : QObject{parent},
      m_timeout{0},
      m_buttons{0},

      m_handle{nullptr},
      m_online{false},
      m_charging{false},
      m_lighting{false},
      m_voltage{false}
{
    loadMaps();

    connect( &m_pollTimer, &QTimer::timeout, this, [this](){
        m_requests.push_back(Voltage);
    } );
    m_pollTimer.setSingleShot(false);
    m_pollTimer.start(5000);

    connect( &m_requestTimer, &QTimer::timeout, this, &HeadsetHID::processRequest );
    m_requestTimer.setSingleShot(false);
    m_requestTimer.start(250);
}

QString HeadsetHID::findDevice()
{
    struct hid_device_info* devs = hid_enumerate(0x0, 0x0);
    struct hid_device_info* cur = devs;

    QString result;
    while( cur )
    {
        if( cur->vendor_id == VENDOR_LOGITECH && cur->product_id == ID_LOGITECH_G733 )
        {
            result = QString::fromLocal8Bit(cur->path);
            break;
        }

        cur = cur->next;
    }
    hid_free_enumeration(devs);

    return result;
}

bool HeadsetHID::open()
{
    // Generate the path which is needed
    QString hid_path = findDevice();
    if( hid_path.isEmpty() )
    {
        qDebug() << "Couldn't find a compatible device.";
        qDebug() << QString::fromWCharArray(hid_error(NULL));
        return false;
    }

    m_handle = hid_open_path(hid_path.toStdString().c_str());
    if( !m_handle )
    {
        qDebug() << "Failed to open the device" << hid_path;
        qDebug() << QString::fromWCharArray(hid_error(NULL));
        return false;
    }

    m_online = true;
    emit onlineChanged(m_online);

    m_requests.push_back(Version);

    return true;
}

void HeadsetHID::close()
{
    if( !m_handle )
    {
        qDebug() << "HeadsetHID::close(): Nothing to do.";
        return;
    }

    hid_close(m_handle);
    m_handle = nullptr;
    m_online = false;
    emit onlineChanged(m_online);
}

int HeadsetHID::voltage()
{
    return m_voltage;
}

int HeadsetHID::soc()
{
    return m_soc;
}

bool HeadsetHID::online()
{
    return m_online;
}

bool HeadsetHID::charging()
{
    return m_charging;
}

bool HeadsetHID::lighting()
{
    return m_lighting;
}

void HeadsetHID::enableLighting(bool onoff)
{
    if( onoff )
    {
        m_requests.push_back(LightsOn);
        m_requests.push_back(Noop);
        m_requests.push_back(Noop);
        m_requests.push_back(Noop);
        m_requests.push_back(Noop);
        m_requests.push_back(LogoOn);
    }
    else
    {
        m_requests.push_back(LightsOff);
        m_requests.push_back(Noop);
        m_requests.push_back(Noop);
        m_requests.push_back(Noop);
        m_requests.push_back(Noop);
        m_requests.push_back(LogoOff);
    }
    m_requests.push_back(Noop);
    m_requests.push_back(Noop);
    m_requests.push_back(Noop);
    m_requests.push_back(Noop);
}

void HeadsetHID::processRequest()
{
    if( m_requests.length() == 0 )
    {
        // Check buttons:
        readFromDevice();
        return;
    }

    RequestType t = m_requests.takeFirst();
    switch( t )
    {
    case Version:
        readVersion();
        readFromDevice();
        break;
    case Voltage:
        readVoltage();
        readFromDevice();
        break;
    case LightsOn:
        setLighting(true);
        readFromDevice();
        break;
    case LogoOn:
        setLogoLighting(true);
        readFromDevice();
        break;
    case LightsOff:
        setLighting(false);
        readFromDevice();
        break;
    case LogoOff:
        setLogoLighting(false);
        readFromDevice();
        break;
    case Noop:
    default:
        break;
    }
}

double HeadsetHID::voltageToSoC(int voltage, bool charging)
{
    QList< QPair<int, double> > &map = m_curve_discharging;
    if( charging )
        map = m_curve_charging;

    if( map.isEmpty() )
        return 0;

    for( QPair<int, double> ent : map )
    {
        if( ent.first <= voltage )
            return ent.second;
    }
    return map.last().second;
}

void HeadsetHID::loadMaps()
{
    m_curve_charging = loadMap(":/maps/charging_ascending.csv", true);
    m_curve_discharging = loadMap(":/maps/discharging.csv", false);
}

QList< QPair<int, double> > HeadsetHID::loadMap(const QString &path, bool reversed)
{
    QList< QPair<int, double> > res;
    QFile f(path);
    if( !f.open(QIODevice::ReadOnly) )
    {
        qCritical() << tr("Failed to open map file \"%1\": %2").arg(path).arg(f.errorString());
        return res;
    }

    QByteArray ba = f.readAll();
    QString str = QString::fromLocal8Bit(ba);
    str = str.remove('\r');
    QStringList lines = str.split('\n');

    for( QString line : lines )
    {
        QStringList parts = line.split(',');
        if( parts.length() < 2 )
            continue;

        QPair<int, double> entry;
        entry.first = parts[0].toInt();
        entry.second = parts[1].toDouble();

        if( reversed )
            res.push_front(entry);
        else
            res.push_back(entry);
    }
    return res;
}

bool HeadsetHID::readyForRequest()
{
    if( !m_handle && !open() )
    {
        if( m_online )
            emit onlineChanged(false);
        m_online = false;
        return false;
    }
    return true;
}

bool HeadsetHID::readVoltage()
{
    /*
        CREDIT GOES TO https://github.com/ashkitten/ for the project
        https://github.com/ashkitten/g933-utils/
        I've simply ported that implementation to this project!
    */
    if( !readyForRequest() )
        return false;

    int r = 0;
    quint8 data_request[HIDPP_LONG_MESSAGE_LENGTH] = { HIDPP_LONG_MESSAGE, HIDPP_DEVICE_RECEIVER, 0x08, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    r = hid_write(m_handle, data_request, sizeof(data_request) / sizeof(data_request[0]));
    if( r < 0 )
    {
        qDebug() << "HeadsetHID::voltage(): Failed to write to headset.";
        qDebug() << QString::fromWCharArray(hid_error(NULL));
        close();
        return false;
    }

    return true;
}

bool HeadsetHID::readVersion()
{
    if( !readyForRequest() )
        return false;

    int r = 0;
    quint8 data_request[HIDPP_LONG_MESSAGE_LENGTH] = { HIDPP_LONG_MESSAGE, HIDPP_DEVICE_RECEIVER, 0x11, 0xff, 0x00, 0x11, 0x00, 0x00, 0xaf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    r = hid_write(m_handle, data_request, sizeof(data_request) / sizeof(data_request[0]));
    if( r < 0 )
    {
        qDebug() << "HeadsetHID::readVersion(): Failed to write to headset.";
        qDebug() << QString::fromWCharArray(hid_error(NULL));
        close();
        return false;
    }

    return true;
}

bool HeadsetHID::readFeatures()
{
    if( !readyForRequest() )
        return false;

    int r = 0;
    quint8 data_request[HIDPP_LONG_MESSAGE_LENGTH] = { HIDPP_LONG_MESSAGE, HIDPP_DEVICE_RECEIVER, 0x08, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    r = hid_write(m_handle, data_request, sizeof(data_request) / sizeof(data_request[0]));
    if( r < 0 )
    {
        qDebug() << "HeadsetHID::readDeviceName(): Failed to write to headset.";
        qDebug() << QString::fromWCharArray(hid_error(NULL));
        close();
        return false;
    }

    return true;
}

bool HeadsetHID::readDeviceName()
{
    if( !readyForRequest() )
        return false;

    int r = 0;
    quint8 data_request[HIDPP_LONG_MESSAGE_LENGTH] = { HIDPP_LONG_MESSAGE, HIDPP_DEVICE_RECEIVER, 0x08, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    r = hid_write(m_handle, data_request, sizeof(data_request) / sizeof(data_request[0]));
    if( r < 0 )
    {
        qDebug() << "HeadsetHID::readDeviceName(): Failed to write to headset.";
        qDebug() << QString::fromWCharArray(hid_error(NULL));
        close();
        return false;
    }

    return true;
}

bool HeadsetHID::setLighting(bool onoff)
{
    // on, breathing  11 ff 04 3c 01 (0 for logo) 02 00 b6 ff 0f a0 00 64 00 00 00
    // off            11 ff 04 3c 01 (0 for logo) 00
    // logo and strips can be controlled individually
    if( !readyForRequest() )
        return false;

    quint8 data_on[HIDPP_LONG_MESSAGE_LENGTH]  = { HIDPP_LONG_MESSAGE, HIDPP_DEVICE_RECEIVER, 0x04, 0x3c, 0x01, 0x02, 0x00, 0xb6, 0xff, 0x0f, 0xa0, 0x00, 0x64, 0x00, 0x00, 0x00 };
    quint8 data_off[HIDPP_LONG_MESSAGE_LENGTH] = { HIDPP_LONG_MESSAGE, HIDPP_DEVICE_RECEIVER, 0x04, 0x3c, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int res = hid_write(m_handle, onoff ? data_on : data_off, HIDPP_LONG_MESSAGE_LENGTH);
    if( res < 0 )
    {
        qDebug() << "HeadsetHID::setLighting(): Write error to headset.";
        qDebug() << QString::fromWCharArray(hid_error(NULL));
        close();
        return false;
    }

    m_lighting = onoff;
    emit lightingChanged(onoff);
    return true;
}

bool HeadsetHID::setLogoLighting(bool onoff)
{
    if( !readyForRequest() )
        return false;

    // turn logo lights on/off
    uint8_t data_logo_on[HIDPP_LONG_MESSAGE_LENGTH]  = { HIDPP_LONG_MESSAGE, HIDPP_DEVICE_RECEIVER, 0x04, 0x3c, 0x00, 0x02, 0x00, 0xb6, 0xff, 0x0f, 0xa0, 0x00, 0x64, 0x00, 0x00, 0x00 };
    uint8_t data_logo_off[HIDPP_LONG_MESSAGE_LENGTH] = { HIDPP_LONG_MESSAGE, HIDPP_DEVICE_RECEIVER, 0x04, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int res = hid_write(m_handle, onoff ? data_logo_on : data_logo_off, HIDPP_LONG_MESSAGE_LENGTH);
    if( res < 0 )
    {
        qDebug() << "HeadsetHID::setLogoLighting(): Write error to headset.";
        qDebug() << QString::fromWCharArray(hid_error(NULL));
        close();
        return false;
    }

    return true;
}

void printPacket(quint8 *data_read, int r)
{
    char obuf[HIDPP_LONG_MESSAGE_LENGTH * 5 + 2];
    obuf[0] = '\0';
    for( int x=0; x < r; x++ )
    {
        char codeout[6];
        snprintf(codeout, sizeof(codeout), " %x", data_read[x]);
        strcat(obuf, codeout);
    }
    printf("Received: %s\n", obuf);
}

void HeadsetHID::readFromDevice()
{
    if( !m_handle )
        return;

    quint8 data_read[7];
    memset( data_read, 0, sizeof(data_read) );
    int r = hid_read_timeout(m_handle, data_read, 7, REQUEST_TIMEOUT);
    if( 0 == r )
    {
        m_timeout++;
        if( m_online && m_timeout >= 20 )
        {
            m_online = false;
            emit onlineChanged(m_online);
            return;
        }
        return;
    }
    else if( !m_online && m_timeout >= 20 )
    {
        m_online = true;
        emit onlineChanged(m_online);
    }
    m_timeout = 0;

    if( r >= 4 && data_read[0] == 0x11 && data_read[1] == 0xff )
    {
        if( r >= 5 && data_read[2] == 0x05 && data_read[3] == 0 )
        {
            uint8_t mask = 1;
            for( int x=0; x < 8; x++ )
            {
                bool wason = (m_buttons & mask);
                bool ison = (data_read[4] & mask);
                if( wason != ison )
                    emit buttonPressed(x, ison);

                mask <<= 1;
            }
            m_buttons = data_read[4];

            return;
        }
        // Battery voltage, seems like a janky way to know SoC.
        else if( r >= 7 )
        {
            if( data_read[2] == 0x08 && data_read[3] == 0x0a )
            {

                quint16 v = (data_read[4] << 8) | data_read[5];
                if( m_voltage != v )
                    emit voltageChanged(v);

                m_voltage = v;

                uint8_t state = data_read[6];
                bool ostate = m_charging;
                if (state == 0x03)
                    m_charging = true;
                else
                    m_charging = false;

                if( ostate != m_charging )
                    emit chargingChanged(m_charging);

                int newSoC = voltageToSoC(m_voltage, m_charging);
                if( newSoC != m_soc )
                    emit socChanged(newSoC);
                m_soc = newSoC;

                return;
            }
            // 11 ff 8 0 0 0 0
            else if( data_read[2] == 0x08 && data_read[3] == 0 && data_read[4] == 0 && data_read[5] == 0 && data_read[6] == 0 )
            {
                printf("Sleeping\n");
                if( m_online )
                {
                    m_online = false;
                    emit onlineChanged(m_online);
                }

                return;
            }
            // 11 ff 8 0 f 83 1 <- 0=sleep, 1=woke?
            //           `--'- important?
            else if( data_read[2] == 0x08 && data_read[3] == 0 && data_read[6] == 0x01 )
            {
                printf("Waking\n");
                if( !m_online )
                {
                    m_online = true;
                    emit onlineChanged(m_online);

                    setLighting(m_lighting);
                }

                return;
            }
            // 11 ff ff 8 a 5 0
            else if( data_read[2] == 0xff && data_read[3] == 0x08 && data_read[4] == 0x0a && data_read[5] == 0x05 && data_read[6] == 0x00 )
            {
                //printf("Timeout?\n");
                //printPacket(data_read, r);

                return;
            }
            // 11 ff 4 3c 1 2 0 // Lights on (breathing mode) confirmed
            else if( data_read[2] == 0x04 && data_read[3] == 0x3c && data_read[6] == 0 )
            {
                bool someon = false;
                if( data_read[4] == 1 && data_read[5] == 2 )
                {
                    someon = true;
                    printf("Lights on (Breathing)\n");
                }
                else if( data_read[4] == 1 && data_read[5] == 0 )
                    printf("Lights off\n");
                else if( data_read[4] == 0 && data_read[5] == 2 )
                {
                    someon = true;
                    printf("Logo on (Breathing)\n");
                }
                else if( data_read[4] == 0 && data_read[5] == 0 )
                    printf("Logo off\n");

                if( someon != m_lighting )
                {
                    printf("Restoring previous lighting state, %s\n", m_lighting ? "on" : "off");
                    setLighting(m_lighting);
                }

                return;
            }
        }
    }

    printf("Unhandled packet:\n");
    printPacket(data_read, r);
}
