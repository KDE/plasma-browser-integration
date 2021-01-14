/*
    SPDX-FileCopyrightText: 2019 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: GPL-3.0-or-later
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
