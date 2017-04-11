#include "abstractbrowserplugin.h"
#include "connection.h"

AbstractBrowserPlugin::AbstractBrowserPlugin::AbstractBrowserPlugin(const QString& subsystemId, int protocolVersion, QObject* parent):
    QObject(parent),
    m_subsystem(subsystemId)
{
    sendData(QStringLiteral("loaded"), {{"version", protocolVersion}});
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

