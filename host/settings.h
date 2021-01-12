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

#pragma once

#include "abstractbrowserplugin.h"

#include <QString>

struct EnvironmentDescription {
    QString applicationName;
    QString applicationDisplayName;
    QString desktopFileName;
    QString organizationDomain;
    QString organizationName;
    QString iconName;
};

namespace TaskManager
{
class WindowTasksModel;
}

/*
 * This class manages the extension's settings (so that settings in the browser
 * propagate to our extension) and also detects the environment the host is run
 * in (e.g. whether we're started by Firefox, Chrome, Chromium, or Opera)
 */
class Settings : public AbstractBrowserPlugin
{
    Q_OBJECT

public:
    static Settings &self();

    enum class Environment {
        Unknown,
        Chrome,
        Chromium,
        Firefox,
        Opera,
        Vivaldi,
        Brave
    };
    Q_ENUM(Environment)

    void handleData(const QString &event, const QJsonObject &data) override;
    QJsonObject handleData(int serial, const QString &event, const QJsonObject &data) override;

    Environment environment() const;

    bool pluginEnabled(const QString &subsystem) const;
    QJsonObject settingsForPlugin(const QString &subsystem) const;

Q_SIGNALS:
    void changed(const QJsonObject &settings);

private:
    Settings();
    ~Settings() override = default;

    bool setEnvironmentFromTasksModelIndex(const QModelIndex &idx);
    void setEnvironmentFromExtensionMessage(const QJsonObject &data);

    static const QMap<Environment, QString> environmentNames;
    static const QMap<Environment, EnvironmentDescription> environmentDescriptions;

    Environment m_environment = Environment::Unknown;
    EnvironmentDescription m_currentEnvironment;

    QJsonObject m_settings;

    TaskManager::WindowTasksModel *m_tasksModel;

};
