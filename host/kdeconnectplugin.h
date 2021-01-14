/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#pragma once
#include "abstractbrowserplugin.h"

class KDEConnectPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT
public:
    explicit KDEConnectPlugin(QObject *parent);
    bool onLoad() override;
    bool onUnload() override;
    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

private Q_SLOTS:
    void onDeviceAdded(const QString &deviceId);
    void onDeviceRemoved(const QString &deviceId);
    void onDeviceVisibilityChanged(const QString &deviceId, bool visible);

private:
    QStringList m_devices;
};

