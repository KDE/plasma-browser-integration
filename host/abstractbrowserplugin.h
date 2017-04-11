#pragma once

#include <QObject>
#include <QJsonObject>
#include <QDebug>

class AbstractBrowserPlugin : public QObject
{
    Q_OBJECT
public:
    ~AbstractBrowserPlugin() = default;
    QString subsystem() const;
    virtual void handleData(const QString &event, const QJsonObject &data);

protected:
    AbstractBrowserPlugin(const QString &subsystemId, QObject *parent);
    void sendData(const QString &action, const QJsonObject &payload = QJsonObject());
    QDebug debug() const;
private:
    QString m_subsystem;
};
