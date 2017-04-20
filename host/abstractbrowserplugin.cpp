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
