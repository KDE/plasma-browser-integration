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

            foreach (const QString &deviceId, devices) {
                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                                  "/modules/kdeconnect/devices/" + deviceId,
                                                                  "org.freedesktop.DBus.Properties",
                                                                  "GetAll");
                msg.setArguments({"org.kde.kdeconnect.device"});
                QDBusPendingReply<QVariantMap> reply = QDBusConnection::sessionBus().asyncCall(msg);
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

                QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, deviceId](QDBusPendingCallWatcher *watcher) {
                    watcher->deleteLater();
                    QDBusPendingReply<QVariantMap> reply = *watcher;
                    if (reply.isError()) {
                        debug() << "getting device properties " + reply.error().message();
                    } else {
                        auto props = reply.value();
                        props["id"] = deviceId;
                        m_devices.append(deviceId);
                        sendData("deviceAdded", QJsonObject::fromVariantMap(props));
                    }
                });
            }
        }
        watcher->deleteLater();
    });
    return true;
}

bool KDEConnectPlugin::onUnload()
{
    foreach(const QString &deviceId, m_devices) {
        sendData("deviceRemoved", {{"id", deviceId}});
    }
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

