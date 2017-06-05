/*
    Copyright (C) 2017 by Kai Uwe Broulik <kde@privat.broulik.de>
    Copyright (C) 2017 by David Edmundson <davidedmundson@kde.org>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include "downloadplugin.h"

#include "connection.h"
#include "downloadjob.h"

#include <KIO/JobTracker>
#include <KJobTrackerInterface>

DownloadPlugin::DownloadPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("downloads"), 1, parent)
{
}

bool DownloadPlugin::onUnload()
{
    for (auto it = m_jobs.constBegin(), end = m_jobs.constEnd(); it != end; ++it) {
        it.value()->deleteLater(); // kill() would abort the download
    }
    return true;
}

void DownloadPlugin::handleData(const QString& event, const QJsonObject& payload)
{
    const QJsonObject &download = payload.value(QStringLiteral("download")).toObject();

    const int id = download.value(QStringLiteral("id")).toInt(-1);
    if (id < 0) {
        qWarning() << "Cannot update download with invalid id" << id;
        return;
    }

    if (event == QLatin1String("created")) {
        auto *job = new DownloadJob(id);

        // first register and then update, otherwise we miss the initial population..
        KIO::getJobTracker()->registerJob(job);

        job->update(download);

        m_jobs.insert(id, job);

        connect(job, &DownloadJob::killRequested, this, [this, id] {
            sendData(QStringLiteral("cancel"), {
                {QStringLiteral("downloadId"), id}
            });
        });

        connect(job, &DownloadJob::suspendRequested, this, [this, id] {
            sendData(QStringLiteral("suspend"), {
                {QStringLiteral("downloadId"), id}
            });
        });

        connect(job, &DownloadJob::resumeRequested, this, [this, id] {
            sendData(QStringLiteral("resume"), {
                {QStringLiteral("downloadId"), id}
            });
        });

        QObject::connect(job, &QObject::destroyed, this, [this, id] {
            m_jobs.remove(id);
        });

        job->start();

        QObject::connect(job, &KJob::finished, this, [this, job, id] {
        });

    } else if (event == QLatin1String("update")) {
        auto *job = m_jobs.value(id);
        if (!job) {
            debug() << "Failed to find download to update with id" << id;
            return;
        }

        job->update(download);
    }
}

