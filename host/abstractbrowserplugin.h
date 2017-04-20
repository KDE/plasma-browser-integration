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

    virtual bool onLoad();
    virtual bool onUnload();
    virtual void onSettingsChanged(const QJsonObject &newSettings);

    bool isLoaded() const;
    // FIXME this should not be public but we need to change it from main.cpp
    void setLoaded(bool loaded);

protected:
    /*
     * @arg subsystemId
     * The name of the plugin. This will be used for the "subsystem" parameter for all data sent
     *
     * @arg protocolVersion
     * As the browser extension will be shipped separately to the native plugin a user could have incompatiable setups
     * Here we inform the browser of the protocol used so if we do ever changed the native API we can at least detect it on the JS side and handle it
     */
    AbstractBrowserPlugin(const QString &subsystemId, int protocolVersion, QObject *parent);
    void sendData(const QString &action, const QJsonObject &payload = QJsonObject());
    QDebug debug() const;

    QJsonObject settings() const;

private:
    QString m_subsystem;
    bool m_loaded = false;
};
