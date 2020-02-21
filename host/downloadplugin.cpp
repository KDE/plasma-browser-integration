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

#include <KLocalizedString>
#include <KNotification>

#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>

#include "itineraryextractorjob.h"

DownloadPlugin::DownloadPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("downloads"), 3, parent)
{
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

void DownloadPlugin::handleData(const QString& event, const QJsonObject& payload)
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

        job = new DownloadJob(id);

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

        QObject::connect(job, &KJob::result, this, [this, job] {
            if (job->error()) {
                return;
            }

            // FIXME check if enabled in settings
            if (job->mimeType() == QLatin1String("application/pdf")) {
                extractItinerary(job->fileName());
            }
        });

        job->start();

    } else if (event == QLatin1String("update")) {
        auto *job = m_jobs.value(id);
        if (!job) {
            debug() << "Failed to find download to update with id" << id;
            return;
        }

        job->update(download);
    }
}

void DownloadPlugin::extractItinerary(const QString &fileName)
{
    auto *job = new ItineraryExtractorJob(fileName);
    job->setInputType(ItineraryExtractorJob::InputType::Pdf);
    job->start();

    connect(job, &KJob::result, /*this*/ [job, fileName] {
        if (job->error()) {
            return;
        }

        const QJsonArray data = QJsonDocument::fromJson(job->extractedData()).array();
        if (data.isEmpty()) {
            return;
        }

        // HACK some nicer notification
        const QString type = data.first().toObject().value(QStringLiteral("@type")).toString();

        QString message = i18n("Would you like to add this file to KDE Itinerary?");
        QString iconName = QStringLiteral("map-globe");
        if (type == QLatin1String("FlightReservation")) {
            message = i18n("Would you like to add this boarding pass to KDE Itinerary?");
            iconName = QStringLiteral("document-send");
        }

        KNotification *noti = KNotification::event(KNotification::Notification,
                                                   i18n("Add to Itinerary"),
                                                   message,
                                                   iconName);
        noti->setHint(QStringLiteral("desktop-entry"), QStringLiteral("org.kde.itinerary"));
        noti->setActions({i18n("Add to Itinerary")});
        connect(noti, &KNotification::action1Activated, [fileName] {
            QProcess::startDetached(QStringLiteral("itinerary"), {fileName});
        });
        noti->sendEvent();
    });
}
