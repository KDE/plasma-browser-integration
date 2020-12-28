/*
 *   Copyright (C) 2020 Kai Uwe Broulik <kde@broulik.de>
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

#include "abstractkrunnerplugin.h"

#include <QDBusMessage>
#include <QMultiHash>

class HistoryRunnerPlugin : public AbstractKRunnerPlugin
{
    Q_OBJECT

public:
    explicit HistoryRunnerPlugin(QObject *parent);

    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

    // DBus API
    RemoteActions Actions() override;
    RemoteMatches Match(const QString &searchTerm) override;
    void Run(const QString &id, const QString &actionId) override;

private:
    QMultiHash<QString, QDBusMessage> m_requests;

};
