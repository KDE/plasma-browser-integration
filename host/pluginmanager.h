/*
    SPDX-FileCopyrightText: 2019 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QObject>

#include <QHash>
#include <QString>

class AbstractBrowserPlugin;
class QJsonObject;

class PluginManager : public QObject
{
    Q_OBJECT

public:
    ~PluginManager() override;

    static PluginManager &self();

    AbstractBrowserPlugin *pluginForSubsystem(const QString &subsystem) const;

    void addPlugin(AbstractBrowserPlugin *plugin);

    bool loadPlugin(AbstractBrowserPlugin *plugin);
    bool unloadPlugin(AbstractBrowserPlugin *plugin);

    void settingsChanged(AbstractBrowserPlugin *plugin, const QJsonObject &settings);

    QStringList knownPluginSubsystems() const;
    QStringList loadedPluginSubsystems() const;

Q_SIGNALS:
    void pluginLoaded(AbstractBrowserPlugin *plugin);
    void pluginUnloaded(AbstractBrowserPlugin *plugin);

private:
    PluginManager();

    void onDataReceived(const QJsonObject &json);
    bool setPluginLoaded(AbstractBrowserPlugin *plugin, bool loaded);

    QHash<QString, AbstractBrowserPlugin *> m_plugins;

};
