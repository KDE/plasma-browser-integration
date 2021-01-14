/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "kdeconnectplugin.h"

#include "connection.h"
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusConnection>

static const QString s_kdeConnectServiceName = QStringLiteral("org.kde.kdeconnect");
static const QString s_kdeConnectObjectPath = QStringLiteral("/modules/kdeconnect");

static const QString s_kdeConnectDaemon = QStringLiteral("org.kde.kdeconnect.daemon");

KDEConnectPlugin::KDEConnectPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("kdeconnect"), 1, parent)
{

}

bool KDEConnectPlugin::onLoad()
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    bus.connect(s_kdeConnectServiceName,
                s_kdeConnectObjectPath,
                s_kdeConnectDaemon,
                QStringLiteral("deviceAdded"),
                this,
                SLOT(onDeviceAdded(QString)));
    bus.connect(s_kdeConnectServiceName,
                s_kdeConnectObjectPath,
                s_kdeConnectDaemon,
                QStringLiteral("deviceRemoved"),
                this,
                SLOT(onDeviceRemoved(QString)));
    bus.connect(s_kdeConnectServiceName,
                   s_kdeConnectObjectPath,
                   s_kdeConnectDaemon,
                   QStringLiteral("deviceVisibilityChanged"),
                   this,
                   SLOT(onDeviceVisibilityChanged(QString,bool)));

    QDBusMessage msg = QDBusMessage::createMethodCall(s_kdeConnectServiceName,
                                                      s_kdeConnectObjectPath,
                                                      s_kdeConnectDaemon,
                                                      QStringLiteral("devices"));
    msg.setArguments({true /* only reachable */, true /* only paired */});
    QDBusPendingReply<QStringList> reply = bus.asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QStringList> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            debug() << "kdeconnect discovery failed:" << reply.error().name();
            return;
        }

        const QStringList devices = reply.value();
        for (const QString &deviceId : devices) {
            onDeviceAdded(deviceId);
        }
    });
    return true;
}

bool KDEConnectPlugin::onUnload()
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    bus.disconnect(s_kdeConnectServiceName,
                   s_kdeConnectObjectPath,
                   s_kdeConnectDaemon,
                   QStringLiteral("deviceAdded"),
                   this,
                   SLOT(onDeviceAdded(QString)));
    bus.disconnect(s_kdeConnectServiceName,
                   s_kdeConnectObjectPath,
                   s_kdeConnectDaemon,
                   QStringLiteral("deviceRemoved"),
                   this,
                   SLOT(onDeviceRemoved(QString)));
    bus.disconnect(s_kdeConnectServiceName,
                   s_kdeConnectObjectPath,
                   s_kdeConnectDaemon,
                   QStringLiteral("deviceVisibilityChanged"),
                   this,
                   SLOT(onDeviceVisibilityChanged(QString,bool)));

    const QStringList devices = m_devices;
    for (const QString &deviceId : devices) {
        onDeviceRemoved(deviceId);
    }
    return true;
}

void KDEConnectPlugin::onDeviceAdded(const QString &deviceId)
{
    if (m_devices.contains(deviceId)) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(s_kdeConnectServiceName,
                                                      QStringLiteral("/modules/kdeconnect/devices/") + deviceId,
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("GetAll"));
    msg.setArguments({QStringLiteral("org.kde.kdeconnect.device")});
    QDBusPendingReply<QVariantMap> reply = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, deviceId](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            debug() << "getting device for properties for" << deviceId << "failed:" << reply.error().message();
            return;
        }

        // We might have gotten a second deviceAdded signal, check again.
        if (m_devices.contains(deviceId)) {
            return;
        }

        QVariantMap props = reply.value();

        if (!props.value(QStringLiteral("isReachable")).toBool()
                || !props.value(QStringLiteral("isTrusted")).toBool()) {
            return;
        }

        props.insert(QStringLiteral("id"), deviceId);

        m_devices.append(deviceId);
        sendData(QStringLiteral("deviceAdded"), QJsonObject::fromVariantMap(props));
    });
}

void KDEConnectPlugin::onDeviceRemoved(const QString &deviceId)
{
    if (m_devices.removeOne(deviceId)) {
        sendData(QStringLiteral("deviceRemoved"), {{QStringLiteral("id"), deviceId}});
    }
}

void KDEConnectPlugin::onDeviceVisibilityChanged(const QString &deviceId, bool visible)
{
    if (visible) {
        onDeviceAdded(deviceId);
    } else {
        onDeviceRemoved(deviceId);
    }
}

void KDEConnectPlugin::handleData(const QString& event, const QJsonObject& json)
{
    if (event == QLatin1String("shareUrl")) {
        const QString deviceId = json.value(QStringLiteral("deviceId")).toString();
        const QString url = json.value(QStringLiteral("url")).toString();

        debug() << "sending kde connect url" << url << "to device" << deviceId;

        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"),
                                                            QStringLiteral("/modules/kdeconnect/devices/") + deviceId + QStringLiteral("/share"),
                                                            QStringLiteral("org.kde.kdeconnect.device.share"),
                                                            QStringLiteral("shareUrl"));
        msg.setArguments({url});
        QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
    }
}

