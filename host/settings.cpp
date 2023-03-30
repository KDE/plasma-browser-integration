/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "settings.h"

#include <unistd.h> // getppid

#include <QDBusConnection>
#include <QGuiApplication>
#include <QIcon>
#include <QProcess>

#include <KDesktopFile>
#include <KProcessList>
#include <KService>

#include <abstracttasksmodel.h>
#include <windowtasksmodel.h>

#include "pluginmanager.h"

#include <config-host.h>

const QMap<Settings::Environment, QString> Settings::environmentNames = {
    {Settings::Environment::Chrome, QStringLiteral("chrome")},
    {Settings::Environment::Chromium, QStringLiteral("chromium")},
    {Settings::Environment::Firefox, QStringLiteral("firefox")},
    {Settings::Environment::Opera, QStringLiteral("opera")},
    {Settings::Environment::Vivaldi, QStringLiteral("vivaldi")},
    {Settings::Environment::Brave, QStringLiteral("brave")},
    {Settings::Environment::Edge, QStringLiteral("edge")},
};

const QMap<Settings::Environment, EnvironmentDescription> Settings::environmentDescriptions = {
    {Settings::Environment::Chrome,
     {
         QStringLiteral("google-chrome"),
         QStringLiteral("Google Chrome"),
         QStringLiteral("google-chrome"),
         QStringLiteral("google.com"),
         QStringLiteral("Google"),
         QStringLiteral("google-chrome"),
     }},
    {Settings::Environment::Chromium,
     {
         QStringLiteral("chromium-browser"),
         QStringLiteral("Chromium"),
         QStringLiteral("chromium-browser"),
         QStringLiteral("google.com"),
         QStringLiteral("Google"),
         QStringLiteral("chromium-browser"),
     }},
    {Settings::Environment::Firefox,
     {
         QStringLiteral("firefox"),
         QStringLiteral("Mozilla Firefox"),
         QStringLiteral("firefox"),
         QStringLiteral("mozilla.org"),
         QStringLiteral("Mozilla"),
         QStringLiteral("firefox"),
     }},
    {Settings::Environment::Opera,
     {
         QStringLiteral("opera"),
         QStringLiteral("Opera"),
         QStringLiteral("opera"),
         QStringLiteral("opera.com"),
         QStringLiteral("Opera"),
         QStringLiteral("opera"),
     }},
    {Settings::Environment::Vivaldi,
     {
         QStringLiteral("vivaldi"),
         QStringLiteral("Vivaldi"),
         // This is what the official package on their website uses
         QStringLiteral("vivaldi-stable"),
         QStringLiteral("vivaldi.com"),
         QStringLiteral("Vivaldi"),
         QStringLiteral("vivaldi"),
     }},
    {Settings::Environment::Brave,
     {
         QStringLiteral("Brave"),
         QStringLiteral("Brave"),
         QStringLiteral("brave-browser"),
         QStringLiteral("brave.com"),
         QStringLiteral("Brave"),
         QStringLiteral("brave"),
     }},
    {Settings::Environment::Edge,
     {
         QStringLiteral("Edge"),
         QStringLiteral("Microsoft Edge"),
         QStringLiteral("microsoft-edge"),
         QStringLiteral("microsoft.com"),
         QStringLiteral("Microsoft"),
         QStringLiteral("microsoft-edge"),
     }},
};

Settings::Settings()
    : AbstractBrowserPlugin(QStringLiteral("settings"), 1, nullptr)
    // Settings is a singleton and cleaned up only in exit_handlers
    // at which point the QPA will already have cleaned up
    // leading to a crash on Wayland
    , m_tasksModel(new TaskManager::WindowTasksModel(qGuiApp))
{
    for (int i = 0; i < m_tasksModel->rowCount(); ++i) {
        if (setEnvironmentFromTasksModelIndex(m_tasksModel->index(i, 0))) {
            break;
        }
    }

    // If we didn't find the browser window yet, monitor the model for changes
    if (qApp->desktopFileName().isEmpty()) {
        connect(m_tasksModel, &TaskManager::WindowTasksModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) {
            if (parent.isValid()) {
                return;
            }

            for (int i = first; i <= last; ++i) {
                if (setEnvironmentFromTasksModelIndex(m_tasksModel->index(i, 0))) {
                    break;
                }
            }
        });
        connect(m_tasksModel,
                &TaskManager::WindowTasksModel::dataChanged,
                this,
                [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
                    // TODO should we bother checking this, even?
                    if (topLeft.parent().isValid() || bottomRight.parent().isValid() || topLeft.column() != 0 || bottomRight.column() != 0) {
                        return;
                    }

                    if (!roles.isEmpty() && !roles.contains(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon)
                        && !roles.contains(TaskManager::AbstractTasksModel::AppId)) {
                        return;
                    }

                    for (int i = topLeft.row(); i <= bottomRight.row(); ++i) {
                        if (setEnvironmentFromTasksModelIndex(m_tasksModel->index(i, 0))) {
                            break;
                        }
                    }
                });
    }
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

        Q_EMIT changed(data);
    } else if (event == QLatin1String("openKRunnerSettings")) {
        QProcess::startDetached(QStringLiteral("systemsettings"), {QStringLiteral("kcm_plasmasearch")});
    } else if (event == QLatin1String("setEnvironment")) {
        setEnvironmentFromExtensionMessage(data);
    }
}

bool Settings::setEnvironmentFromTasksModelIndex(const QModelIndex &idx)
{
    bool ok = false;
    const auto pid = idx.data(TaskManager::AbstractTasksModel::AppPid).toLongLong(&ok);
    if (!ok || pid != getppid()) {
        return false;
    }

    const QUrl launcherUrl = idx.data(TaskManager::AbstractTasksModel::LauncherUrlWithoutIcon).toUrl();
    if (!launcherUrl.isValid()) {
        return false;
    }

    KService::Ptr service;
    if (launcherUrl.scheme() == QLatin1String("applications")) {
        service = KService::serviceByMenuId(launcherUrl.path());
    } else if (launcherUrl.isLocalFile()) {
        const QString launcherPath = launcherUrl.toLocalFile();
        if (KDesktopFile::isDesktopFile(launcherPath)) {
            service = KService::serviceByDesktopPath(launcherUrl.toLocalFile());
        }
    } else {
        qWarning() << "Got unrecognized launcher URL" << launcherUrl;
        return false;
    }

    if (!service) {
        qWarning() << "Failed to get service from launcher URL" << launcherUrl;
        return false;
    }

    // Ignore any browser-hosted app windows.
    if (!service->categories().contains(QLatin1String("WebBrowser"))) {
        qInfo() << "Ignoring launcher URL" << launcherUrl << "which doesn't have \"WebBrowser\" category";
        return false;
    }

    qApp->setApplicationName(service->menuId());
    qApp->setApplicationDisplayName(service->name());
    qApp->setDesktopFileName(service->desktopEntryName());
    qApp->setWindowIcon(QIcon::fromTheme(service->icon()));

    m_tasksModel->disconnect(this); // prevent further signal emission to not deref a nullptr https://bugs.kde.org/show_bug.cgi?id=435811
    m_tasksModel->deleteLater();
    m_tasksModel = nullptr;

    return true;
}

void Settings::setEnvironmentFromExtensionMessage(const QJsonObject &data)
{
    QString name = data.value(QStringLiteral("browserName")).toString();

    // Most chromium-based browsers just impersonate Chromium nowadays to keep websites from locking them out
    // so we'll need to make an educated guess from our parent process
    if (name == QLatin1String("chromium") || name == QLatin1String("chrome")) {
        const auto processInfo = KProcessList::processInfo(getppid());
        if (processInfo.name().contains(QLatin1String("vivaldi"))) {
            name = QStringLiteral("vivaldi");
        } else if (processInfo.name().contains(QLatin1String("brave"))) {
            name = QStringLiteral("brave");
        } else if (processInfo.name().contains(QLatin1String("edge"))) {
            name = QStringLiteral("edge");
        }
    }

    m_environment = Settings::environmentNames.key(name, Settings::Environment::Unknown);
    m_currentEnvironment = Settings::environmentDescriptions.value(m_environment);

    if (qApp->desktopFileName().isEmpty()) {
        qApp->setApplicationName(m_currentEnvironment.applicationName);
        qApp->setApplicationDisplayName(m_currentEnvironment.applicationDisplayName);
        qApp->setDesktopFileName(m_currentEnvironment.desktopFileName);
        qApp->setWindowIcon(QIcon::fromTheme(m_currentEnvironment.iconName));
        // TODO remove?
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

bool Settings::pluginEnabled(const QString &subsystem) const
{
    return settingsForPlugin(subsystem).value(QStringLiteral("enabled")).toBool();
}

QJsonObject Settings::settingsForPlugin(const QString &subsystem) const
{
    return m_settings.value(subsystem).toObject();
}
