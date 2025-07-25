/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "downloadplugin.h"

#include "connection.h"
#include "settings.h"

#include <KInhibitionJobTracker>
#include <KUiServerV2JobTracker>

DownloadPlugin::DownloadPlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("downloads"), 4, parent)
    , m_uiServerTracker(new KUiServerV2JobTracker(this))
{
}

void DownloadPlugin::onSettingsChanged(const QJsonObject &settings)
{
    if (settings.value(QStringLiteral("inhibitSuspend")).toBool()) {
        if (!m_inhibitionTracker) {
            m_inhibitionTracker = new KInhibitionJobTracker(this);
        }
        for (auto *job : std::as_const(m_jobs)) {
            m_inhibitionTracker->registerJob(job);
        }
    } else {
        delete m_inhibitionTracker;
        m_inhibitionTracker = nullptr;
    }
}

bool DownloadPlugin::onLoad()
{
    // Have extension tell us about all the downloads
    sendData(QStringLiteral("createAll"));
    return true;
}

bool DownloadPlugin::onUnload()
{
    for (auto it = m_jobs.constBegin(), end = m_jobs.constEnd(); it != end; ++it) {
        it.value()->deleteLater(); // kill() would abort the download
    }
    return true;
}

void DownloadPlugin::handleData(const QString &event, const QJsonObject &payload)
{
    const QJsonObject &download = payload.value(QStringLiteral("download")).toObject();

    const int id = download.value(QStringLiteral("id")).toInt(-1);
    if (id < 0) {
        qWarning() << "Cannot update download with invalid id" << id;
        return;
    }

    if (event == QLatin1String("created")) {
        // If we get a created event for an already existing job, update it instead
        auto *job = m_jobs.value(id);
        if (job) {
            job->update(download);
            return;
        }

        job = new DownloadJob();

        // first register and then update, otherwise it will miss the initial description() emission
        m_uiServerTracker->registerJob(job);

        if (m_inhibitionTracker) {
            m_inhibitionTracker->registerJob(job);
        }

        job->update(download);

        m_jobs.insert(id, job);

        connect(job, &DownloadJob::killRequested, this, [this, id] {
            sendData(QStringLiteral("cancel"),
                     {
                         {QStringLiteral("downloadId"), id},
                     });
        });

        connect(job, &DownloadJob::suspendRequested, this, [this, id] {
            sendData(QStringLiteral("suspend"),
                     {
                         {QStringLiteral("downloadId"), id},
                     });
        });

        connect(job, &DownloadJob::resumeRequested, this, [this, id] {
            sendData(QStringLiteral("resume"),
                     {
                         {QStringLiteral("downloadId"), id},
                     });
        });

        QObject::connect(job, &QObject::destroyed, this, [this, id] {
            m_jobs.remove(id);
        });

        job->start();

        QObject::connect(job, &KJob::finished, this, [this, job, id] {});

    } else if (event == QLatin1String("update")) {
        auto *job = m_jobs.value(id);
        if (!job) {
            debug() << "Failed to find download to update with id" << id;
            return;
        }

        job->update(download);
    }
}

#include "moc_downloadplugin.cpp"
