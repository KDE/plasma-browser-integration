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
