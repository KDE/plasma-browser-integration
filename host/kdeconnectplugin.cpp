#include "kdeconnectplugin.h"

#include "connection.h"
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusConnection>

KDEConnectPlugin::KDEConnectPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("kdeconnect"), 1, parent)
{

}

bool KDEConnectPlugin::onLoad()
{
    debug() << "kdeconnect" << "querying";

    QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                      "/modules/kdeconnect",
                                                      "org.kde.kdeconnect.daemon",
                                                      "devices");
    msg.setArguments({true /* only reachable */, true /* only paired */});
    QDBusPendingReply<QStringList> reply = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QStringList> reply = *watcher;
        if (reply.isError()) {
            debug() << "kdeconnect discovery" << reply.error().name();
        } else {
            const QStringList &devices = reply.value();
            QString defaultDevice;
            if (!devices.isEmpty()) {
                defaultDevice = devices.first();
            }
            if (!devices.isEmpty()) {
                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                                  "/modules/kdeconnect/devices/" + defaultDevice,
                                                                  "org.freedesktop.DBus.Properties",
                                                                  "Get");
                msg.setArguments({"org.kde.kdeconnect.device", "name"});
                QDBusPendingReply<QDBusVariant> reply = QDBusConnection::sessionBus().asyncCall(msg);
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
                QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, defaultDevice](QDBusPendingCallWatcher *watcher) {
                    QDBusPendingReply<QDBusVariant> reply = *watcher;
                    if (reply.isError()) {
                        debug() << "query default name " + reply.error().message();
                    } else {
                        const QString name = reply.value().variant().toString();
                        sendData("devicesChanged", {{"defaultDeviceName", name}, {"defaultDeviceId", defaultDevice}});
                    }
                    watcher->deleteLater();
                });
            }
        }
        watcher->deleteLater();
    });
    return true;
}

bool KDEConnectPlugin::onUnload()
{
    // pretend we don't have any devices anymore, this simplifies context menu handling significantly
    sendData("devicesChanged");
    return true;
}

void KDEConnectPlugin::handleData(const QString& event, const QJsonObject& json)
{
    if (event == QLatin1String("shareUrl")) {
            const QString deviceId = json.value("deviceId").toString();
            const QString url = json.value("url").toString();

            debug() << "sending kde connect url" << url << "to device" << deviceId;

            QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                                "/modules/kdeconnect/devices/" + deviceId + "/share",
                                                                "org.kde.kdeconnect.device.share",
                                                                "shareUrl");
            msg.setArguments({url});
            QDBusPendingReply<QStringList> reply = QDBusConnection::sessionBus().asyncCall(msg);

        }
}

