/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
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
        Brave,
        Edge
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
