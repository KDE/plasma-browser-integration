#pragma once

#include <QObject>
#include <QJsonObject>

class AbstractBrowserPlugin : public QObject
{
    Q_OBJECT
public:
    ~AbstractBrowserPlugin() = default;
    QString subsystem() const;
    virtual void handleData(const QString &event, const QJsonObject &data);
    void sendData(const QJsonObject &data);
protected:
    AbstractBrowserPlugin(const QString &subsystemId, QObject *parent);
private:
    QString m_subsystem;
};
