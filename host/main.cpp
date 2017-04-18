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

    QList<AbstractBrowserPlugin*> m_plugins;
    m_plugins << &Settings::self();
    m_plugins << &WindowMapper::self();
    m_plugins << new IncognitoPlugin(&a);
    m_plugins << new KDEConnectPlugin(&a);
    m_plugins << new DownloadPlugin(&a);
    m_plugins << new TabsRunnerPlugin(&a);
    m_plugins << new MPrisPlugin(&a);
    m_plugins << new SlcPlugin(&a);

    // TODO pid suffix or so if we want to allow multiple extensions (which we probably should)
    if (!QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.plasma.browser_integration"))) {
        qWarning() << "Failed to register DBus service";
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
            if (plugin->subsystem() == subsystem) {
                //design question, should we have a JSON of subsystem, event, payload, or have all data at the root level?
                plugin->handleData(event, json);
                return;
            }
        }

        qDebug() << "Don't know how to handle event" << event << "for subsystem" << subsystem;
    });

    return a.exec();
}

#include "main.moc"
