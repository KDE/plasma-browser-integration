#include <QApplication>
#include <QDBusConnection>
#include <QDebug>

#include "connection.h"
#include "abstractbrowserplugin.h"

#include "settings.h"
#include "windowmapper.h"
#include "incognitoplugin.h"
#include "kdeconnectplugin.h"
#include "downloadplugin.h"
#include "tabsrunnerplugin.h"
#include "mprisplugin.h"
#include "slcplugin.h"

void msgHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);

    QJsonObject data;
    data["subsystem"] = QStringLiteral("debug");
    switch(type) {
        case QtDebugMsg:
        case QtInfoMsg:
            data["action"] = "debug";
            break;
        default:
            data["action"] = "warning";
    }
    data["payload"] = QJsonObject({{"message", msg}});

    Connection::self()->sendData(data);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(msgHandler);

    QApplication a(argc, argv);
    // otherwise will close when download job finishes
    a.setQuitOnLastWindowClosed(false);

    a.setApplicationName("google-chrome");
    a.setApplicationDisplayName("Google Chrome");

    // NOTE if you add a new plugin here, make sure to adjust the
    // "DEFAULT_EXTENSION_SETTINGS" in constants.js or else it won't
    // even bother loading your shiny new plugin!
    QList<AbstractBrowserPlugin*> m_plugins;
    m_plugins << &Settings::self();
    m_plugins << &WindowMapper::self();
    m_plugins << new IncognitoPlugin(&a);
    m_plugins << new KDEConnectPlugin(&a);
    m_plugins << new DownloadPlugin(&a);
    m_plugins << new TabsRunnerPlugin(&a);
    m_plugins << new MPrisPlugin(&a);
    m_plugins << new SlcPlugin(&a);

    // TODO make this prettier, also prevent unloading them at any cost
    Settings::self().setLoaded(true);
    WindowMapper::self().setLoaded(true);

    QString serviceName = QStringLiteral("org.kde.plasma.browser_integration");
    if (!QDBusConnection::sessionBus().registerService(serviceName)) {
        // now try appending PID in case multiple hosts are running
        serviceName.append(QLatin1String("-")).append(QString::number(QCoreApplication::applicationPid()));
        if (!QDBusConnection::sessionBus().registerService(serviceName)) {
            qWarning() << "Failed to register DBus service name" << serviceName;
        }
    }

    QObject::connect(Connection::self(), &Connection::dataReceived, [m_plugins](const QJsonObject &json) {
        const QString subsystem = json.value(QStringLiteral("subsystem")).toString();

        if (subsystem.isEmpty()) {
            qDebug() << "No subsystem provided";
            return;
        }

        const QString event = json.value(QStringLiteral("event")).toString();
        if (event.isEmpty()) {
            qDebug() << "No event provided";
            return;
        }

        foreach(AbstractBrowserPlugin *plugin, m_plugins) {
            if (!plugin->isLoaded()) {
                continue;
            }

            if (plugin->subsystem() == subsystem) {
                //design question, should we have a JSON of subsystem, event, payload, or have all data at the root level?
                plugin->handleData(event, json);
                return;
            }
        }

        qDebug() << "Don't know how to handle event" << event << "for subsystem" << subsystem;
    });

    QObject::connect(&Settings::self(), &Settings::changed, [m_plugins](const QJsonObject &settings) {
        foreach(AbstractBrowserPlugin *plugin, m_plugins) {
            const QJsonValue &val = settings.value(plugin->subsystem());
            if (val.type() != QJsonValue::Object) {
                qWarning() << "Plugin" << plugin->subsystem() << "not handled by settings change";
                continue;
            }

            // FIXME let a plugin somehow tell that it must not be unloaded
            if (plugin->subsystem() == QLatin1String("settings") || plugin->subsystem() == QLatin1String("windows")) {
                continue;
            }

            const QJsonObject &settingsObject = val.toObject();

            const bool enabled = settingsObject.value(QStringLiteral("enabled")).toBool();
            bool ok = false;

            if (enabled && !plugin->isLoaded()) {
                ok = plugin->onLoad();
                if (!ok) {
                    qWarning() << "Plugin" << plugin->subsystem() << "refused to load";
                }
            } else if (!enabled && plugin->isLoaded()) {
                ok = plugin->onUnload();
                if (!ok) {
                    qWarning() << "Plugin" << plugin->subsystem() << "refused to unload";
                }
            }

            if (ok) {
                plugin->setLoaded(enabled);
            }

            plugin->onSettingsChanged(settingsObject);
        }
    });

    return a.exec();
}
