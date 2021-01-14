/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "abstractbrowserplugin.h"
#include "connection.h"
#include "settings.h"

AbstractBrowserPlugin::AbstractBrowserPlugin::AbstractBrowserPlugin(const QString& subsystemId, int protocolVersion, QObject* parent):
    QObject(parent),
    m_subsystem(subsystemId),
    m_protocolVersion(protocolVersion)
{

}

void AbstractBrowserPlugin::handleData(const QString& event, const QJsonObject& data)
{
    Q_UNUSED(event);
    Q_UNUSED(data);
}

QJsonObject AbstractBrowserPlugin::handleData(int serial, const QString &event, const QJsonObject &data)
{
    Q_UNUSED(serial);
    Q_UNUSED(event);
    Q_UNUSED(data);
    return QJsonObject();
}

void AbstractBrowserPlugin::sendData(const QString &action, const QJsonObject &payload)
{
    QJsonObject data;
    data[QStringLiteral("subsystem")] = m_subsystem;
    data[QStringLiteral("action")] = action;
    if (!payload.isEmpty()) {
        data[QStringLiteral("payload")] = payload;
    }
    Connection::self()->sendData(data);
}

void AbstractBrowserPlugin::sendReply(int requestSerial, const QJsonObject &payload)
{
    QJsonObject data{
        {QStringLiteral("replyToSerial"), requestSerial},
    };

    if (!payload.isEmpty()) {
        data.insert(QStringLiteral("payload"), payload);
    }

    Connection::self()->sendData(data);
}

bool AbstractBrowserPlugin::onLoad()
{
    return true;
}

bool AbstractBrowserPlugin::onUnload()
{
    return true;
}

void AbstractBrowserPlugin::onSettingsChanged(const QJsonObject &newSettings)
{
    Q_UNUSED(newSettings);
}

QJsonObject AbstractBrowserPlugin::status() const
{
    return {};
}

QDebug AbstractBrowserPlugin::debug() const
{
    auto d = qDebug();
    QDebugStateSaver saver(d);
    d.nospace().noquote() << m_subsystem << ":";
    return d;
}

QString AbstractBrowserPlugin::subsystem() const
{
    return m_subsystem;
}

int AbstractBrowserPlugin::protocolVersion() const
{
    return m_protocolVersion;
}

bool AbstractBrowserPlugin::isLoaded() const
{
    return m_loaded;
}

void AbstractBrowserPlugin::setLoaded(bool loaded)
{
    m_loaded = loaded;
}

QJsonObject AbstractBrowserPlugin::settings() const
{
    return Settings::self().settingsForPlugin(m_subsystem);
}
