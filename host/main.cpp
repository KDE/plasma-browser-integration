#include <QApplication>
#include <QDebug>

#include "mpris.h"
#include "connection.h"
#include "abstractbrowserplugin.h"
#include "incognitoplugin.h"
#include "kdeconnectplugin.h"
#include "downloadplugin.h"

void msgHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    Connection::self()->sendError(msg);
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
    m_plugins << new IncognitoPlugin(&a);
    m_plugins << new KDEConnectPlugin(&a);
    m_plugins << new DownloadPlugin(&a);

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
                qDebug() << "handling";
                //design question, should we have a JSON of subsystem, event, payload, or have all data at the root level?
                plugin->handleData(event, json);
                return;
            }
        }
        qDebug() << "not handling";

    });

    return a.exec();
}

#include "main.moc"
