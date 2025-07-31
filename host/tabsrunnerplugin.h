/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include "abstractkrunnerplugin.h"

#include <QDBusMessage>
#include <QJsonArray>
#include <QMultiHash>

class TabsRunnerPlugin : public AbstractKRunnerPlugin
{
    Q_OBJECT

public:
    explicit TabsRunnerPlugin(QObject *parent);

    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

    // DBus API
    RemoteActions Actions() override;
    RemoteMatches Match(const QString &searchTerm) override;
    void Run(const QString &id, const QString &actionId) override;
    void Teardown() override;

private:
    RemoteMatches match(const QString &query) const;

    QMultiHash<QString, QDBusMessage> m_requests;
    QJsonArray m_tabs;
};
