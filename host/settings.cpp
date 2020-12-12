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

#include "settings.h"

#include <unistd.h> // getppid

#include <QGuiApplication>
#include <QDBusConnection>
#include <QProcess>

#include <KProcessList>

#include "pluginmanager.h"
#include "settingsadaptor.h"

#include <config-host.h>

const QMap<Settings::Environment, QString> Settings::environmentNames = {
    {Settings::Environment::Chrome, QStringLiteral("chrome")},
    {Settings::Environment::Chromium, QStringLiteral("chromium")},
    {Settings::Environment::Firefox, QStringLiteral("firefox")},
    {Settings::Environment::Opera, QStringLiteral("opera")},
    {Settings::Environment::Vivaldi, QStringLiteral("vivaldi")},
    {Settings::Environment::Brave, QStringLiteral("brave")}
};

const QMap<Settings::Environment, EnvironmentDescription> Settings::environmentDescriptions = {
    {Settings::Environment::Chrome, {
        QStringLiteral("google-chrome"),
        QStringLiteral("Google Chrome"),
        QStringLiteral("google-chrome"),
        QStringLiteral("google.com"),
        QStringLiteral("Google"),
        QStringLiteral("google-chrome")
    } },
    {Settings::Environment::Chromium, {
        QStringLiteral("chromium-browser"),
        QStringLiteral("Chromium"),
        QStringLiteral("chromium-browser"),
        QStringLiteral("google.com"),
        QStringLiteral("Google"),
        QStringLiteral("chromium-browser")
    } },
    {Settings::Environment::Firefox, {
        QStringLiteral("firefox"),
        QStringLiteral("Mozilla Firefox"),
        QStringLiteral("firefox"),
        QStringLiteral("mozilla.org"),
        QStringLiteral("Mozilla"),
        QStringLiteral("firefox")
    } },
    {Settings::Environment::Opera, {
        QStringLiteral("opera"),
        QStringLiteral("Opera"),
        QStringLiteral("opera"),
        QStringLiteral("opera.com"),
        QStringLiteral("Opera"),
        QStringLiteral("opera")
    } },
    {Settings::Environment::Vivaldi, {
        QStringLiteral("vivaldi"),
        QStringLiteral("Vivaldi"),
        // This is what the official package on their website uses
        QStringLiteral("vivaldi-stable"),
        QStringLiteral("vivaldi.com"),
        QStringLiteral("Vivaldi"),
        QStringLiteral("vivaldi")
    } },
    {Settings::Environment::Brave, {
        QStringLiteral("Brave"),
        QStringLiteral("Brave"),
        QStringLiteral("brave-browser"),
        QStringLiteral("brave.com"),
        QStringLiteral("Brave"),
        QStringLiteral("brave")
    } }
};

Settings::Settings()
    : AbstractBrowserPlugin(QStringLiteral("settings"), 1, nullptr)
{
    new SettingsAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Settings"), this);

}

Settings &Settings::self()
{
    static Settings s_self;
    return s_self;
}

void Settings::handleData(const QString &event, const QJsonObject &data)
{
    if (event == QLatin1String("changed")) {
        m_settings = data;

        for (auto it = data.begin(), end = data.end(); it != end; ++it) {
            const QString &subsystem = it.key();
            const QJsonObject &settingsObject = it->toObject();

            const QJsonValue enabledVariant = settingsObject.value(QStringLiteral("enabled"));
            // probably protocol overhead, not a plugin setting, skip.
            if (enabledVariant.type() == QJsonValue::Undefined) {
                continue;
            }

            auto *plugin = PluginManager::self().pluginForSubsystem(subsystem);
            if (!plugin) {
                continue;
            }

            if (enabledVariant.toBool()) {
                PluginManager::self().loadPlugin(plugin);
            } else {
                PluginManager::self().unloadPlugin(plugin);
            }

            PluginManager::self().settingsChanged(plugin, settingsObject);
        }

        emit changed(data);
    } else if (event == QLatin1String("openKRunnerSettings")) {
        QProcess::startDetached(QStringLiteral("kcmshell5"), {QStringLiteral("kcm_plasmasearch")});
    } else if (event == QLatin1String("setEnvironment")) {
        QString name = data[QStringLiteral("browserName")].toString();

        // Most chromium-based browsers just impersonate Chromium nowadays to keep websites from locking them out
        // so we'll need to make an educated guess from our parent process
        if (name == QLatin1String("chromium") || name == QLatin1String("chrome")) {
            const auto processInfo = KProcessList::processInfo(getppid());
            if (processInfo.name().contains(QLatin1String("vivaldi"))) {
                name = QStringLiteral("vivaldi");
            } else if (processInfo.name().contains(QLatin1String("brave"))) {
                name = QStringLiteral("brave");
            }
        }

        m_environment = Settings::environmentNames.key(name, Settings::Environment::Unknown);
        m_currentEnvironment = Settings::environmentDescriptions.value(m_environment);

        qApp->setApplicationName(m_currentEnvironment.applicationName);
        qApp->setApplicationDisplayName(m_currentEnvironment.applicationDisplayName);
        qApp->setDesktopFileName(m_currentEnvironment.desktopFileName);
        qApp->setOrganizationDomain(m_currentEnvironment.organizationDomain);
        qApp->setOrganizationName(m_currentEnvironment.organizationName);
    }
}

QJsonObject Settings::handleData(int serial, const QString &event, const QJsonObject &data)
{
    Q_UNUSED(serial)
    Q_UNUSED(data)

    QJsonObject ret;

    if (event == QLatin1String("getSubsystemStatus")) {
       // should we add a PluginManager::knownSubsystems() that returns a QList<AbstractBrowserPlugin*>?
       const QStringList subsystems = PluginManager::self().knownPluginSubsystems();
       for (const QString &subsystem : subsystems) {
           const AbstractBrowserPlugin *plugin = PluginManager::self().pluginForSubsystem(subsystem);

           QJsonObject details = plugin->status();
           details.insert(QStringLiteral("version"), plugin->protocolVersion());
           details.insert(QStringLiteral("loaded"), plugin->isLoaded());
           ret.insert(subsystem, details);
        }
    } else if (event == QLatin1String("getVersion")) {
        ret.insert(QStringLiteral("host"), QStringLiteral(HOST_VERSION_STRING));
    }

    return ret;
}

Settings::Environment Settings::environment() const
{
    return m_environment;
}

QString Settings::environmentString() const
{
    return Settings::environmentNames.value(m_environment);
}

EnvironmentDescription Settings::environmentDescription() const
{
    return m_currentEnvironment;
}

bool Settings::pluginEnabled(const QString &subsystem) const
{
    return settingsForPlugin(subsystem).value(QStringLiteral("enabled")).toBool();
}

QJsonObject Settings::settingsForPlugin(const QString &subsystem) const
{
    return m_settings.value(subsystem).toObject();
}
