#include <QApplication>

#include <QTextStream>
#include <QDebug>
#include <QFile>

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

static QHash<int, DownloadJob *> s_jobs;

static KStatusNotifierItem *s_incognitoItem = nullptr;

static void sendData(const QJsonObject &data)
{
    const QByteArray rawData = QJsonDocument(data).toJson(QJsonDocument::Compact);

    unsigned int len = rawData.count();

    std::cerr << "Sende data of length" << len;

    char first = char(len >> 0);
    char second = char(len >> 8);
    char third = char(len >> 16);
    char fourth = char(len >> 24);

    setbuf(stdout, nullptr); // needed?
/*
    write(1, &first, 1);
    write(1, &second, 1);
    write(1, &third, 1);
    write(1, &fourth, 1);

    write(1, partyData.constData(), partyData.count());*/

    std::cout << first << second << third << fourth << rawData.constData();

    std::cerr << errno;
}

static void sendError(const QString &error, const QJsonObject &info = QJsonObject())
{
    QJsonObject data = info;
    data.insert(QStringLiteral("error"), error);
    sendData(data);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // otherwise will close when download job finishes
    a.setQuitOnLastWindowClosed(false);

    a.setApplicationName("google-chrome");
    a.setApplicationDisplayName("Google Chrome");

    /*QFile partyFile("/home/broulik/partyfilee");
    partyFile.open(QIODevice::WriteOnly | QIODevice::Append);

    QTextStream stream(stdin);
    QString line;
    while (stream.readLineInto(&line)) {
        partyFile.write(line.toUtf8());
    }*/

    //setmode(fileno(stdout), O_BINARY);

    QFile partyFile("/home/broulik/partyfilee");
    partyFile.open(QIODevice::WriteOnly | QIODevice::Append);

    //QFile stdinfile;
    //stdinfile.open(stdin, QIODevice::ReadOnly);

    //QDataStream party(&stdinfile);

    QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, [] {
        QFile stdinfile;
        stdinfile.open(stdin, QIODevice::ReadOnly | QIODevice::Unbuffered);

        /*if (stdinfile.bytesAvailable() < 5) { // doesn't make sense, always needs to have a header
            std::cerr << "Received message which is too short (5 bytes needed at least)";
            sendData({ {"error", "bytes available too small"}, {"bytesAvailable", stdinfile.bytesAvailable()} });
            return;
        }*/

        unsigned int length = 0;

        for (int i = 0; i < 4; ++i) {
            char readChar;
            stdinfile.read(&readChar, 1);
            length |= (readChar << (i * 8));
        }

        // TODO sanity checks for size and what not

        QByteArray data = stdinfile.read(length);// = stdinfile.readAll();
        int amount = data.count();
        //qint64 amount = stdinfile.read(data.data(), length);

        //sendData({ {"ARRIVED", (int)length}, {"amount", amount}, {"data", QString::fromUtf8(data)} });

        if (!amount) {// || amount != length) {
            sendData({ {"error", "amount not match"}, {"amount", amount}, {"len", (int)length } });
            return;
        }

        QJsonObject json = QJsonDocument::fromJson(data).object();
        if (json.isEmpty()) {
            sendData({ {"error", "json empty or parse error"}, {"data was", QString::fromUtf8(data) }, {"l", data.length()}, {"LEN", (int)length} });
            return;
        }

        //const int msgId = json.value("msg").toInt(-1);
        //sendData({ { "success", true}, {"msgnr", msgId} });

        const QString subsystem = json.value(QStringLiteral("subsystem")).toString();

        if (subsystem.isEmpty()) {
            sendError("No subsystem provided");
            return;
        }

        const QString event = json.value(QStringLiteral("event")).toString();

        if (subsystem == QLatin1String("downloads")) {
            const int id = json.value(QStringLiteral("id")).toInt(-1);
            if (id < 0) {
                sendError("download id invalid", { {"id", id} });
                return;
            }

            const QJsonObject &payload = json.value(QStringLiteral("payload")).toObject();

            if (event == QLatin1String("created")) {
                auto *job = new DownloadJob(id);
                job->update(payload);

                KIO::getJobTracker()->registerJob(job);

                sendData({ {"download begins", id}, {"payload", payload} });

                s_jobs.insert(id, job);

                QObject::connect(job, &QObject::destroyed, [id] {
                    sendData({ {"download job destroyed", id} });
                    s_jobs.remove(id);
                    sendData({ {"download job removed", id} });
                });

                job->start();

                QObject::connect(job, &KJob::finished, [job, id] {
                    sendData({ {"job finished", id}, {"error", job->error()} });
                });


            } else if (event == QLatin1String("update")) {

                auto *job = s_jobs.value(id);
                if (!job) {
                    sendError("failed to find download id to update", { {"id", id} });
                    return;
                }

                sendData({ {"download update ABOUT TO", id}, {"payload", payload} });

                job->update(payload);

                sendData({ {"download update DONE", id}, {"payload", payload} });
            }
        } else if (subsystem == QLatin1String("mpris")) {

            if (event == QLatin1String("play")) {
                sendData({ {"mpris play", true} });
            } else if (event == QLatin1String("pause")) {
                sendData({ {"mpris pause", true} });
            }

        } else if (subsystem == QLatin1String("incognito")) {

            if (event == QLatin1String("show")) {

                if (s_incognitoItem) {
                    sendData({ {"incognito already there", true} });
                    return;
                }

                s_incognitoItem = new KStatusNotifierItem();

                s_incognitoItem->setIconByName("face-smirk");
                s_incognitoItem->setTitle("Incognito Tabs");
                s_incognitoItem->setStandardActionsEnabled(false);
                s_incognitoItem->setStatus(KStatusNotifierItem::Active);

                QMenu *menu = new QMenu();

                QAction *closeAllAction = menu->addAction(QIcon::fromTheme("window-close"), "Close all Incognito Tabs");
                QObject::connect(closeAllAction, &QAction::triggered, [] {
                    sendData({ {"subsystem", "incognito"}, {"action", "close"} });
                });

                s_incognitoItem->setContextMenu(menu);

                sendData({ {"incognito indicator", true} });

            } else if (event == QLatin1String("hide")) {

                if (!s_incognitoItem) {
                    sendData({ {"no incongito there but wanted to hide", true} });
                    return;
                }

                delete s_incognitoItem;
                s_incognitoItem = nullptr;

                sendData({ {"incognito indicator", false} });
            }

        }




        /*partyFile.write(stdinfile.readAll());
        partyFile.flush();
        partyFile.close();*/
        /*QTextStream party(stdin);
        party.setCodec("UTF-8");
        partyFile.write(party.readAll().toUtf8());
        partyFile.close();*/

        //sendParty();
    });

    new MPris(&a);

    int nr = 0;


    /*QTimer timer;
    timer.setInterval(1000);
    QObject::connect(&timer, &QTimer::timeout, [&nr] {
        sendData({ {"party", ++nr} });
    });
    timer.start();*/

    //QTimer::singleShot(1000, [] {


        /*QFile file("/home/broulik/partyfilee");
        file.open(stdout, QIODevice::WriteOnly);
        //file.open(QIODevice::WriteOnly);

        uint32_t bla = partyData.count();;

        uint32_t result = 0;


        {
            QDataStream stream(&file);
            stream.setByteOrder(QDataStream::LittleEndian);

            result |= (bla & 0x000000FF);// << 24;
            result |= (bla & 0x0000FF00);// << 8;
            result |= (bla & 0x00FF0000);// >> 8;
            result |= (bla & 0xFF000000);// >> 24;

            //stream << quint32(partyData.count());//2 ;// 0 << 0 << quint32(partyData.count());

            stream << char(0) << char(0) << char(0) << char(5) << partyData;

                   //stream << quint32(partyData.count()) <<partyData;


        file.close();*/




        //QTextStream stream(stdout);

/*        int nMsgLen = partyData.length();

             const char first = (unsigned char)(nMsgLen & 0x000000ff);
            const char second = (unsigned char)((nMsgLen >> 8) & 0x000000ff);
            const char third = (unsigned char)((nMsgLen >> 16) & 0x000000ff);
             const char fourth = (unsigned char)((nMsgLen >> 24) & 0x000000ff);

        QFile file;
        file.open(stdout, QIODevice::WriteOnly);
        //file.write(&first);
        //file.write(&second);
        //file.write(&third);
        //file.write(&fourth);

        file.write(0);
        file.write(0);
        file.write(0);
        file.write(reinterpret_cast<const char*>(0x2));

        file.write(partyData);
        file.close();*/



        //stream.flush();
        //return;


        //stream << first << second << third << fourth << partyData;
        //stream << __bswap_32(nMsgLen) << partyData;
       //stream << quint32(5) << "{\"hallo\"}";
       //stream << __bswap_32() << "{\"hallo\": \"du\"}";
        //stream.flush();

/*
        QFile partyFile("/home/broulik/partyfilee");
        partyFile.open(QIODevice::WriteOnly | QIODevice::Append);*/

        //QByteArray bla;
        //QTextStream partyStream(bla);

        //partyStream << first << second << third << fourth << partyData;
        //partyStream.flush();

        //partyFile.write(bla);
        //partyFile.write(first);
        //partyFile.write(second);
        //partyFile.write(third);
        //partyFile.write(fourth);
        //partyFile.write(partyData);*/


    //});

    return a.exec();
}
