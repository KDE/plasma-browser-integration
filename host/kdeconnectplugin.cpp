#include "kdeconnectplugin.h"

#include "connection.h"
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusConnection>

KDEConnectPlugin::KDEConnectPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("kdeconnect"), parent)
{
    sendData({ {"subsystem", "kdeconnect" }, {"status", "querying" } });
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
            Connection::self()->sendError("kdeconnect discovery", { {"error", reply.error().name()} });
        } else {
            const QStringList &devices = reply.value();
            QString defaultDevice;
            if (!devices.isEmpty()) {
                defaultDevice = devices.first();
            }
            sendData({ {"subsystem", "kdeconnect"}, {"status", "finished querying default device"}, {"defaultDeviceId", defaultDevice} });

            if (!devices.isEmpty()) {
                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                                  "/modules/kdeconnect/devices/" + defaultDevice,
                                                                  "org.freedesktop.DBus.Properties",
                                                                  "Get");
                msg.setArguments({"org.kde.kdeconnect.device", "name"});
                QDBusPendingReply<QDBusVariant> reply = QDBusConnection::sessionBus().asyncCall(msg);
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
                QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
                    QDBusPendingReply<QDBusVariant> reply = *watcher;
                    if (reply.isError()) {
                        Connection::self()->sendError("kdeconnect query default name " + reply.error().message());
                    } else {
                        const QString name = reply.value().variant().toString();
                        sendData({ {"subsystem", "kdeconnect"}, {"status", "finished querying default device"}, {"defaultDeviceName", name} });
                    }
                    watcher->deleteLater();
                });
            }
        }
        watcher->deleteLater();
    });
}

void KDEConnectPlugin::handleData(const QString& event, const QJsonObject& json)
{
    if (event == QLatin1String("shareUrl")) {
            const QString &deviceId = json.value("deviceId").toString();
            const QString &url = json.value("url").toString();

            sendData({ {"send kde connect url", url}, {"to device", deviceId} });

            QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                                "/modules/kdeconnect/devices/" + deviceId + "/share",
                                                                "org.kde.kdeconnect.device.share",
                                                                "shareUrl");
            msg.setArguments({json.value("url").toString()});
            QDBusPendingReply<QStringList> reply = QDBusConnection::sessionBus().asyncCall(msg);

        }
}

