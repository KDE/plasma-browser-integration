#include "downloadplugin.h"

#include "connection.h"
#include "downloadjob.h"

#include <KIO/JobTracker>
#include <KJobTrackerInterface>

DownloadPlugin::DownloadPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("downloads"), 1, parent)
{
}

void DownloadPlugin::handleData(const QString& event, const QJsonObject& json)
{
    const int id = json.value(QStringLiteral("id")).toInt(-1);
    if (id < 0) {
        debug() << "download id invalid, id:" << id;
        return;
    }

    const QJsonObject &payload = json.value(QStringLiteral("payload")).toObject();

    if (event == QLatin1String("created")) {
        auto *job = new DownloadJob(id);
        job->update(payload);

        KIO::getJobTracker()->registerJob(job);

        debug() << "download begins" << id << "payload" << payload;

        m_jobs.insert(id, job);

        QObject::connect(job, &QObject::destroyed, this, [this, id] {
            m_jobs.remove(id);
        });

        job->start();

        QObject::connect(job, &KJob::finished, this, [this, job, id] {
        });


    } else if (event == QLatin1String("update")) {
        auto *job = m_jobs.value(id);
        if (!job) {
            debug() << "failed to find download id to update ID: "<< id;
            return;
        }

        debug() << "download update ABOUT TO" << id << "payload" << payload;

        job->update(payload);

        debug() << "download update DONE" << id << "payload" << payload;
    }
}

