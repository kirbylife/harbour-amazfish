#include "asteroidosdevice.h"
#include "batteryservice.h"
#include "asteroidtimeservice.h"

#include <QtXml/QtXml>


AsteroidOSDevice::AsteroidOSDevice(const QString &pairedName, QObject *parent) : AbstractDevice(pairedName, parent)
{
    qDebug() << Q_FUNC_INFO << pairedName;
    connect(this, &QBLEDevice::propertiesChanged, this, &AsteroidOSDevice::onPropertiesChanged, Qt::UniqueConnection);
}

int AsteroidOSDevice::supportedFeatures() const
{
    return 0;
//FEATURE_HRM | FEATURE_ALERT | FEATURE_WEATHER | FEATURE_STEPS;
}

QString AsteroidOSDevice::deviceType() const
{
    return "asteroidos";
}

AbstractFirmwareInfo *AsteroidOSDevice::firmwareInfo(const QByteArray &bytes)
{
    qDebug() << Q_FUNC_INFO;
    return nullptr;
}

void AsteroidOSDevice::sendAlert(const QString &sender, const QString &subject, const QString &message)
{
    qDebug() << Q_FUNC_INFO << sender << subject << message;
}

void AsteroidOSDevice::incomingCall(const QString &caller)
{
    qDebug() << Q_FUNC_INFO << caller;
}


void AsteroidOSDevice::pair()
{
    qDebug() << Q_FUNC_INFO;

    m_needsAuth = true;
    m_pairing = true;
    m_autoreconnect = true;
    //disconnectFromDevice();
    setConnectionState("pairing");
    emit connectionStateChanged();

    QBLEDevice::connectToDevice();
}


/*
void AsteroidOSDevice::pair() override;

void AsteroidOSDevice::connectToDevice() override;
void AsteroidOSDevice::disconnectFromDevice() override;
QString AsteroidOSDevice::connectionState() const;
int AsteroidOSDevice::supportedFeatures() const = 0;
QString AsteroidOSDevice::deviceType() const = 0;
void AsteroidOSDevice::abortOperations();

//Firmware handling
AbstractFirmwareInfo * AsteroidOSDevice::firmwareInfo(const QByteArray &bytes) = 0; //Caller owns the pointer and should delete it
void AsteroidOSDevice::prepareFirmwareDownload(const AbstractFirmwareInfo* info);
void AsteroidOSDevice::startDownload();

void AsteroidOSDevice::downloadSportsData();
void AsteroidOSDevice::sendWeather(CurrentWeather *weather);
void AsteroidOSDevice::refreshInformation();
QString AsteroidOSDevice::information(Info i) const;
void AsteroidOSDevice::applyDeviceSetting(Settings s);
void AsteroidOSDevice::rebootWatch();
void AsteroidOSDevice::sendAlert(const QString &sender, const QString &subject, const QString &message) = 0;
void AsteroidOSDevice::incomingCall(const QString &caller) = 0;
void AsteroidOSDevice::sendEventReminder(int id, const QDateTime &dt, const QString &event);
void AsteroidOSDevice::enableFeature(AbstractDevice::Feature feature);
void AsteroidOSDevice::setMusicStatus(bool playing, const QString &title, const QString &artist, const QString &album, int duration = 0, int position = 0);
void AsteroidOSDevice::navigationRunning(bool running);
void AsteroidOSDevice::navigationNarrative(const QString &flag, const QString &narrative, const QString &manDist, int progress);
QStringList AsteroidOSDevice::supportedDisplayItems() const;
*/


void AsteroidOSDevice::onPropertiesChanged(QString interface, QVariantMap map, QStringList list)
{
    qDebug() << Q_FUNC_INFO << interface << map << list;

    if (interface == "org.bluez.Device1") {
        m_reconnectTimer->start();
        if (deviceProperty("ServicesResolved").toBool() ) {
            initialise();
        }
        if (map.contains("Connected")) {
            bool value = map["Connected"].toBool();

            if (!value) {
                qDebug() << "DisConnected!";
                setConnectionState("disconnected");
            } else {
                setConnectionState("connected");
            }
        }
    }

}


void AsteroidOSDevice::parseServices()
{
    qDebug() << Q_FUNC_INFO;

    QDBusInterface adapterIntro("org.bluez", devicePath(), "org.freedesktop.DBus.Introspectable", QDBusConnection::systemBus(), 0);
    QDBusReply<QString> xml = adapterIntro.call("Introspect");

    qDebug() << "Resolved services...";

    qDebug().noquote() << xml.value();

    QDomDocument doc;
    doc.setContent(xml.value());

    QDomNodeList nodes = doc.elementsByTagName("node");

    qDebug() << nodes.count() << "nodes";

    for (int x = 0; x < nodes.count(); x++)
    {
        QDomElement node = nodes.at(x).toElement();
        QString nodeName = node.attribute("name");

        if (nodeName.startsWith("service")) {
            QString path = devicePath() + "/" + nodeName;

            QDBusInterface devInterface("org.bluez", path, "org.bluez.GattService1", QDBusConnection::systemBus(), 0);
            QString uuid = devInterface.property("UUID").toString();

            qDebug() << "Creating service for: " << uuid;

            if (uuid == BatteryService::UUID_SERVICE_BATTERY && !service(BatteryService::UUID_SERVICE_BATTERY)) {
                addService(BatteryService::UUID_SERVICE_BATTERY, new BatteryService(path, this));
            } else if (uuid == AsteroidTimeService::UUID_SERVICE_ASTEROID_TIME && !service(AsteroidTimeService::UUID_SERVICE_ASTEROID_TIME)) {
                addService(AsteroidTimeService::UUID_SERVICE_ASTEROID_TIME, new AsteroidTimeService(path, this));

            } else if ( !service(uuid)) {
                addService(uuid, new QBLEService(uuid, path, this));
            }
        }
    }
    setConnectionState("authenticated");
}

void AsteroidOSDevice::initialise()
{
    qDebug() << Q_FUNC_INFO;
    setConnectionState("connected");
    parseServices();


    BatteryService *battery = qobject_cast<BatteryService*>(service(BatteryService::UUID_SERVICE_BATTERY));
    if (battery) {
qDebug() << "connect(battery, &BatteryService::informationChanged, this, &AsteroidOSDevice::informationChanged, Qt::UniqueConnection);";
        connect(battery, &BatteryService::informationChanged, this, &AsteroidOSDevice::informationChanged, Qt::UniqueConnection);
    }


    AsteroidTimeService *ats = qobject_cast<AsteroidTimeService*>(service(AsteroidTimeService::UUID_SERVICE_ASTEROID_TIME));
    if (ats) {
        ats->setCurrentTime();
    }


/*
    PineTimeMusicService *ms = qobject_cast<PineTimeMusicService*>(service(PineTimeMusicService::UUID_SERVICE_MUSIC));
    if (ms) {
        ms->enableNotification(PineTimeMusicService::UUID_CHARACTERISTIC_MUSIC_EVENT);
        connect(ms, &PineTimeMusicService::serviceEvent, this, &AsteroidOSDevice::serviceEvent, Qt::UniqueConnection);
    }
*/
}


void AsteroidOSDevice::authenticated(bool ready)
{
    qDebug() << Q_FUNC_INFO << ready;

    if (ready) {
        setConnectionState("authenticated");
    } else {
        setConnectionState("authfailed");
    }
}

void AsteroidOSDevice::refreshInformation()
{

    BatteryService *bat = qobject_cast<BatteryService*>(service(BatteryService::UUID_SERVICE_BATTERY));
    if (bat) {
        bat->refreshInformation();
    }

}

