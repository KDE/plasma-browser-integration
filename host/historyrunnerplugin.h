/*
    SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-3.0-or-later
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
