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

#pragma once

#include "abstractbrowserplugin.h"

#include <QDBusContext>
#include <QDBusMessage>

class NotificationFilterPlugin : public AbstractBrowserPlugin, protected QDBusContext
{
    Q_OBJECT

public:
    explicit NotificationFilterPlugin(QObject *parent);
    ~NotificationFilterPlugin() override;

    bool onLoad() override;
    bool onUnload() override;

    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

    // dbus-exported
    bool IsServiceRunning(const QString &service, int &granted);

private:
    QString m_requestedService;
    QDBusMessage m_request;

};
