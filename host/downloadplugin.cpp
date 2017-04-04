#include "downloadplugin.h"

#include "connection.h"
#include "downloadjob.h"

#include <KIO/JobTracker>
#include <KJobTrackerInterface>

DownloadPlugin::DownloadPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("downloads"), parent)
{
}

void DownloadPlugin::handleData(const QString& event, const QJsonObject& json)
{
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

        m_jobs.insert(id, job);

        QObject::connect(job, &QObject::destroyed, this, [this, id] {
            Connection::self()->sendData({ {"download job destroyed", id} });
            m_jobs.remove(id);
            Connection::self()->sendData({ {"download job removed", id} });
        });

        job->start();

        QObject::connect(job, &KJob::finished, this, [this, job, id] {
            Connection::self()->sendData({ {"job finished", id}, {"error", job->error()} });
        });


    } else if (event == QLatin1String("update")) {
        auto *job = m_jobs.value(id);
        if (!job) {
            Connection::self()->sendError("failed to find download id to update", { {"id", id} });
            return;
        }

        Connection::self()->sendData({ {"download update ABOUT TO", id}, {"payload", payload} });

        job->update(payload);

        Connection::self()->sendData({ {"download update DONE", id}, {"payload", payload} });
    }
}

