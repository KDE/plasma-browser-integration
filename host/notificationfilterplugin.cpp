/*
    Copyright (C) 2019 Kai Uwe Broulik <kde@privat.broulik.de>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "notificationfilterplugin.h"

#include <QDBusConnection>
#include <QDebug>

#include "notificationfilteradaptor.h"

NotificationFilterPlugin::NotificationFilterPlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("notificationfilter"), 1, parent)
{
    new NotificationFilterAdaptor(this);
}

NotificationFilterPlugin::~NotificationFilterPlugin() = default;

bool NotificationFilterPlugin::onLoad()
{
    return QDBusConnection::sessionBus().registerObject(QStringLiteral("/NotificationFilter"), this);
}

bool NotificationFilterPlugin::onUnload()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/NotificationFilter"));

    m_request = QDBusMessage();
    m_requestedService.clear();

    return true;
}

bool NotificationFilterPlugin::IsServiceRunning(const QString &service, int &granted)
{
    if (m_request.type() != QDBusMessage::InvalidMessage) {
        sendErrorReply(
            QStringLiteral("org.kde.plasma.browser_integration.NotificationFilter.Error.Busy"),
            QStringLiteral("Another request is already being processed")
        );
        return false;
    }

    m_request = message();
    m_requestedService = service;
    sendData(QStringLiteral("checkService"), {
        {QStringLiteral("service"), service}
    });
    setDelayedReply(true);

    // ignored because delayedReply
    granted = false;
    return false;
}

void NotificationFilterPlugin::handleData(const QString &event, const QJsonObject &data)
{
    const QString service = data.value(QStringLiteral("service")).toString();

    if (service == m_requestedService) {

        if (event == QLatin1String("gotService")) {
            static const QHash<QString, int> s_permissionMapping = {
                {QStringLiteral("default"), -1},
                {QStringLiteral("denied"), 0},
                {QStringLiteral("granted"), 1}
            };

            const bool running = data.value(QStringLiteral("running")).toBool();

            const QString permissionString = data.value(QStringLiteral("notificationPermission")).toString();
            // In doubt return -2 as "error determining permission"
            const int permission = s_permissionMapping.value(permissionString, -2);

            QDBusConnection::sessionBus().send(m_request.createReply({
                running, permission
            }));

        } else if (event == QLatin1String("unknownService")) {

            QDBusConnection::sessionBus().send(m_request.createErrorReply(
                QStringLiteral("org.kde.plasma.browser_integration.NotificationFilter.Error.ServiceUnknown"),
                QStringLiteral("The requested service is not known")
            ));

        }

    } else {
        // shouldn't happen
        qWarning() << "Got service reply" << service << "for requested" << m_requestedService;
    }

    m_request = QDBusMessage();
    m_requestedService.clear();
}
