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
    m_subsystem(subsystemId)
{
    sendData(QStringLiteral("created"), {{"version", protocolVersion}});
}

void AbstractBrowserPlugin::handleData(const QString& event, const QJsonObject& data)
{
    Q_UNUSED(event);
    Q_UNUSED(data);
}

void AbstractBrowserPlugin::sendData(const QString &action, const QJsonObject &payload)
{
    QJsonObject data;
    data["subsystem"] = m_subsystem;
    data["action"] = action;
    if (!payload.isEmpty()) {
        data["payload"] = payload;
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

QDebug AbstractBrowserPlugin::debug() const
{
    auto d = qDebug();
    d << m_subsystem << ": ";
    return d;
}

QString AbstractBrowserPlugin::subsystem() const
{
    return m_subsystem;
}

bool AbstractBrowserPlugin::isLoaded() const
{
    return m_loaded;
}

void AbstractBrowserPlugin::setLoaded(bool loaded)
{
    if (m_loaded == loaded) {
        return;
    }

    if (loaded) {
        sendData(QStringLiteral("loaded"));
    } else {
        sendData(QStringLiteral("unloaded"));
    }

    m_loaded = loaded;
}

QJsonObject AbstractBrowserPlugin::settings() const
{
    return Settings::self().settingsForPlugin(m_subsystem);
}
