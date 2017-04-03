#include <QApplication>

#include <QTextStream>
#include <QDebug>
#include <QFile>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>

#include <QJsonObject>
#include <QJsonDocument>

#include <QMenu>
#include <QTimer>

#include <stdio.h>

#include <iostream>

#include <QDataStream>

#include <QSocketNotifier>

#include <unistd.h>

#include "downloadjob.h"

#include <KIO/JobTracker>
#include <KJobTrackerInterface>

#include <KStatusNotifierItem>

#include "mpris.h"
#include "connection.h"
#include "abstractbrowserplugin.h"
#include "incognitoplugin.h"

static QHash<int, DownloadJob *> s_jobs;

static KStatusNotifierItem *s_incognitoItem = nullptr;

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

    QObject::connect(Connection::self(), &Connection::dataReceived, [m_plugins](const QJsonObject &json) {
        if (json.isEmpty()) {
            return;
        }
        const QString subsystem = json.value(QStringLiteral("subsystem")).toString();

        if (subsystem.isEmpty()) {
            Connection::self()->sendError("No subsystem provided");
            return;
        }

        const QString event = json.value(QStringLiteral("event")).toString();

        foreach(AbstractBrowserPlugin *plugin, m_plugins) {
            if (plugin->subsystem() == subsystem) {
                //design question, should we have a JSON of subsystem, event, payload, or have all data at the root level?
                plugin->handleData(event, json);
            }
        }

        if (subsystem == QLatin1String("downloads")) {
            const int id = json.value(QStringLiteral("id")).toInt(-1);
            if (id < 0) {
                Connection::self()->sendError("download id invalid", { {"id", id} });
                return;
            }

            const QJsonObject &payload = json.value(QStringLiteral("payload")).toObject();

            if (event == QLatin1String("created")) {
                auto *job = new DownloadJob(id);
                job->update(payload);

                KIO::getJobTracker()->registerJob(job);

                Connection::self()->sendData({ {"download begins", id}, {"payload", payload} });

                s_jobs.insert(id, job);

                QObject::connect(job, &QObject::destroyed, [id] {
                    Connection::self()->sendData({ {"download job destroyed", id} });
                    s_jobs.remove(id);
                    Connection::self()->sendData({ {"download job removed", id} });
                });

                job->start();

                QObject::connect(job, &KJob::finished, [job, id] {
                    Connection::self()->sendData({ {"job finished", id}, {"error", job->error()} });
                });


            } else if (event == QLatin1String("update")) {

                auto *job = s_jobs.value(id);
                if (!job) {
                    Connection::self()->sendError("failed to find download id to update", { {"id", id} });
                    return;
                }

                Connection::self()->sendData({ {"download update ABOUT TO", id}, {"payload", payload} });

                job->update(payload);

                Connection::self()->sendData({ {"download update DONE", id}, {"payload", payload} });
            }
        } else if (subsystem == QLatin1String("mpris")) {

            if (event == QLatin1String("play")) {
                Connection::self()->sendData({ {"mpris play", true} });
            } else if (event == QLatin1String("pause")) {
                Connection::self()->sendData({ {"mpris pause", true} });
            }
        } else if (subsystem == QLatin1String("kdeconnect")) {
            Connection::self()->sendError("ACK");
            if (event == QLatin1String("shareUrl")) {

                const QString &deviceId = json.value("deviceId").toString();
                const QString &url = json.value("url").toString();

                Connection::self()->sendData({ {"send kde connect url", url}, {"to device", deviceId} });

                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                                  "/modules/kdeconnect/devices/" + deviceId + "/share",
                                                                  "org.kde.kdeconnect.device.share",
                                                                  "shareUrl");
                msg.setArguments({json.value("url").toString()});
                QDBusPendingReply<QStringList> reply = QDBusConnection::sessionBus().asyncCall(msg);

            }

        }
    });

    new MPris(&a);

    int nr = 0;

    Connection::self()->sendData({ {"subsystem", "kdeconnect" }, {"status", "querying" } });
    QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                      "/modules/kdeconnect",
                                                      "org.kde.kdeconnect.daemon",
                                                      "devices");
    msg.setArguments({true /* only reachable */, true /* only paired */});
    QDBusPendingReply<QStringList> reply = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, [](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QStringList> reply = *watcher;
        if (reply.isError()) {
            Connection::self()->sendError("kdeconnect discovery", { {"error", reply.error().name()} });
        } else {
            const QStringList &devices = reply.value();
            QString defaultDevice;
            if (!devices.isEmpty()) {
                defaultDevice = devices.first();
            }
            Connection::self()->sendData({ {"subsystem", "kdeconnect"}, {"status", "finished querying default device"}, {"defaultDeviceId", defaultDevice} });

            if (!devices.isEmpty()) {
                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect",
                                                                  "/modules/kdeconnect/devices/" + defaultDevice,
                                                                  "org.freedesktop.DBus.Properties",
                                                                  "Get");
                msg.setArguments({"org.kde.kdeconnect.device", "name"});
                QDBusPendingReply<QDBusVariant> reply = QDBusConnection::sessionBus().asyncCall(msg);
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
                QObject::connect(watcher, &QDBusPendingCallWatcher::finished, [](QDBusPendingCallWatcher *watcher) {
                    QDBusPendingReply<QDBusVariant> reply = *watcher;
                    if (reply.isError()) {
                        Connection::self()->sendError("kdeconnect query default name " + reply.error().message());
                    } else {
                        const QString name = reply.value().variant().toString();
                        Connection::self()->sendData({ {"subsystem", "kdeconnect"}, {"status", "finished querying default device"}, {"defaultDeviceName", name} });
                    }
                    watcher->deleteLater();
                });
            }
        }
        watcher->deleteLater();
    });
    return a.exec();
}

#include "main.moc"
