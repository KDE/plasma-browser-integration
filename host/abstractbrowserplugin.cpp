#include "abstractbrowserplugin.h"
#include "connection.h"

AbstractBrowserPlugin::AbstractBrowserPlugin::AbstractBrowserPlugin(const QString& subsystemId, QObject* parent):
    QObject(parent),
    m_subsystem(subsystemId)
{
}

void AbstractBrowserPlugin::handleData(const QString& event, const QJsonObject& data)
{
    Q_UNUSED(event);
    Q_UNUSED(data);
}

void AbstractBrowserPlugin::sendData(const QJsonObject& data)
{
    Connection::self()->sendData(data);
}

QString AbstractBrowserPlugin::subsystem() const
{
    return m_subsystem;
}

