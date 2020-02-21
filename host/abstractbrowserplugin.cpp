/*
    Copyright (C) 2017 by Kai Uwe Broulik <kde@privat.broulik.de>
    Copyright (C) 2017 by David Edmundson <davidedmundson@kde.org>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
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

QDebug AbstractBrowserPlugin::warning() const
{
    auto d = qWarning();
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
