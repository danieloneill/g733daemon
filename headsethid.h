#ifndef HEADSETHID_H
#define HEADSETHID_H

#include <QList>
#include <QObject>
#include <QTimer>

#include <hidapi.h>

#define REQUEST_TIMEOUT 100 // in ms

class HeadsetHID : public QObject
{
    Q_OBJECT

    typedef enum {
        Noop,
        DeviceName,
        Features,
        LightsOn,
        LogoOn,
        LightsOff,
        LogoOff,
        Version,
        Voltage
    } RequestType;

    int         m_timeout;
    quint8      m_buttons;
    QTimer      m_pollTimer;
    QTimer      m_requestTimer;

    QList<RequestType> m_requests;

public:
    explicit HeadsetHID(QObject *parent = nullptr);

    static QString findDevice();
    bool open();
    void close();

public slots:
    int voltage();
    int soc();
    bool online();
    bool charging();
    bool lighting();
    void enableLighting(bool onoff);

protected:
    hid_device  *m_handle;

    bool m_online;
    bool m_charging;
    bool m_lighting;
    quint16 m_voltage;
    quint16 m_soc;

    QList< QPair<int, double> > m_curve_discharging;
    QList< QPair<int, double> > m_curve_charging;

    double voltageToSoC(int voltage, bool charging);
    QList< QPair<int, double> > loadMap(const QString &path, bool reversed);
    void loadMaps();

    bool readyForRequest();

private slots:
    bool readVersion();
    bool readVoltage();
    bool readFeatures();
    bool readDeviceName();
    bool setLighting(bool onoff);
    bool setLogoLighting(bool onoff);
    void processRequest();
    void readFromDevice();

signals:
    void chargingChanged(bool onoff);
    void onlineChanged(bool onoff);
    void voltageChanged(double voltage);
    void socChanged(int soc);
    void lightingChanged(bool onoff);
    void buttonPressed(int index, bool pressed);
};

#endif // HEADSETHID_H
