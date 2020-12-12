/*
 *   Copyright (C) 2019 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 3 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "abstractbrowserplugin.h"

#include <QDBusContext>
#include <QDBusMessage>
#include <QMultiHash>

// FIXME adaptor needs this, can we tell qt5_add_dbus_adaptor to add it to the generated file?
#include "dbusutils_p.h"

class HistoryRunnerPlugin : public AbstractBrowserPlugin, protected QDBusContext
{
    Q_OBJECT
    // FIXME do we need this? TabsRunner doesn't set one
    Q_CLASSINFO("D-Bus Interface", "org.kde.plasma.browser_integration.HistoryRunner")

public:
    explicit HistoryRunnerPlugin(QObject *parent);
    bool onLoad() override;
    bool onUnload() override;

    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

    // DBus API
    RemoteActions Actions();
    RemoteMatches Match(const QString &searchTerm);
    void Run(const QString &id, const QString &actionId);

    // dbus-exported
    //QList<QHash<QString, QVariant>> GetTabs();
    //void Activate(int tabId);
    //void SetMuted(int tabId, bool muted);

private:
    QMultiHash<QString, QDBusMessage> m_requests;

};
