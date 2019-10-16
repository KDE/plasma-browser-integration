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

#include "pluginmanager.h"

#include <QJsonObject>

#include "abstractbrowserplugin.h"
#include "connection.h"

PluginManager::PluginManager()
    : QObject(nullptr)
{
    connect(Connection::self(), &Connection::dataReceived, this, &PluginManager::onDataReceived);
}

PluginManager::~PluginManager() = default;

PluginManager &PluginManager::self()
{
    static PluginManager s_self;
    return s_self;
}

AbstractBrowserPlugin *PluginManager::pluginForSubsystem(const QString &subsystem) const
{
    return m_plugins.value(subsystem);
}

void PluginManager::addPlugin(AbstractBrowserPlugin *plugin)
{
    m_plugins.insert(plugin->m_subsystem, plugin);
}

bool PluginManager::loadPlugin(AbstractBrowserPlugin *plugin)
{
    return setPluginLoaded(plugin, true);
}

bool PluginManager::unloadPlugin(AbstractBrowserPlugin *plugin)
{
    return setPluginLoaded(plugin, false);
}

bool PluginManager::setPluginLoaded(AbstractBrowserPlugin *plugin, bool loaded)
{
    if (!plugin) {
        return false;
    }

    if (plugin->isLoaded() == loaded) {
        return true;
    }

    bool ok = false;
    if (loaded && !plugin->isLoaded()) {
        ok = plugin->onLoad();
    } else if (!loaded && plugin->isLoaded()) {
        ok = plugin->onUnload();
    }

    if (!ok) {
        qDebug() << "plugin" << plugin->subsystem() << "refused to" << (loaded ? "load" : "unload");
        return false;
    }

    plugin->setLoaded(loaded);

    if (loaded) {
        emit pluginLoaded(plugin);
    } else {
        emit pluginUnloaded(plugin);
    }

    return true;
}

void PluginManager::settingsChanged(AbstractBrowserPlugin *plugin, const QJsonObject &settings)
{
    plugin->onSettingsChanged(settings);
}

QStringList PluginManager::knownPluginSubsystems() const
{
    return m_plugins.keys();
}

QStringList PluginManager::loadedPluginSubsystems() const
{
    QStringList plugins;
    for (auto it = m_plugins.constBegin(), end = m_plugins.constEnd(); it != end; ++it) {
        if (it.value()->isLoaded()) {
            plugins << it.key();
        }
    }
    return plugins;
}

void PluginManager::onDataReceived(const QJsonObject &json)
{
    const QString subsystem = json.value(QStringLiteral("subsystem")).toString();

    AbstractBrowserPlugin *plugin = m_plugins.value(subsystem);
    if (!plugin) {
        return;
    }

    if (!plugin->isLoaded()) {
        return;
    }

    const QString event = json.value(QStringLiteral("event")).toString();
    if (event.isEmpty()) {
        return;
    }

    const QJsonValue requestSerialVariant = json.value(QStringLiteral("serial"));
    if (!requestSerialVariant.isUndefined()) {
        const int requestSerial = requestSerialVariant.toInt();

        const auto reply = plugin->handleData(requestSerial, event, json);
        if (!reply.isEmpty()) {
            plugin->sendReply(requestSerial, reply);
        }
    } else {
        //design question, should we have a JSON of subsystem, event, payload, or have all data at the root level?
        plugin->handleData(event, json);
    }
}
