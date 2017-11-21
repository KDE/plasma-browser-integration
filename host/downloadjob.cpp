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

#include "downloadjob.h"
#include "settings.h"

#include <QJsonObject>

#include <KLocalizedString>

#include <KIO/Global>

DownloadJob::DownloadJob(int id)
    : KJob()
    , m_id(id)
{
    // the thing with "canResume" in chrome downloads is that it just means
    // "this download can be resumed right now because it is paused",
    // it's not a general thing. I think we can always pause/resume downloads
    // unless they're canceled/interrupted at which point we don't have a DownloadJob
    // anymore anyway
    setCapabilities(Killable | Suspendable);

    // TODO When suspending on Firefox the download job goes away for some reason?!
    // Until I have the virtue to figure that out just disallow suspending downloads on Firefox :)
    if (Settings::self().environment() == Settings::Environment::Firefox) {
        setCapabilities(Killable);
    }
}

void DownloadJob::start()
{
    QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
}

void DownloadJob::doStart()
{

}

bool DownloadJob::doKill()
{
    emit killRequested();
    // TODO what if the user kills us from notification area while the
    // "Save As" prompt is still open?
    return true;
}

bool DownloadJob::doSuspend()
{
    emit suspendRequested();
    return true;
}

bool DownloadJob::doResume()
{
    emit resumeRequested();
    return true;
}

void DownloadJob::update(const QJsonObject &payload)
{
    auto end = payload.constEnd();

    bool descriptionDirty = false;

    auto it = payload.constFind(QStringLiteral("url"));
    if (it != end) {
        m_url = QUrl(it->toString());
        descriptionDirty = true; // TODO only if actually changed
    }

    it = payload.constFind(QStringLiteral("finalUrl"));
    if (it != end) {
        m_finalUrl = QUrl(it->toString());
        descriptionDirty = true;
    }

    it = payload.constFind(QStringLiteral("filename"));
    if (it != end) {
        m_destination = QUrl::fromLocalFile(it->toString());
        descriptionDirty = true;
    }

    it = payload.constFind(QStringLiteral("totalBytes"));
    if (it != end) {
        setTotalAmount(Bytes, it->toDouble());
    }

    it = payload.constFind(QStringLiteral("bytesReceived"));
    if (it != end) {
        setProcessedAmount(Bytes, it->toDouble());
    }

    it = payload.constFind(QStringLiteral("paused"));
    if (it != end) {
        const bool paused = it->toBool();

        if (paused) {
            suspend();
        } else {
            resume();
        }
    }

    it = payload.constFind(QStringLiteral("estimatedEndTime"));
    if (it != end) {
        qulonglong speed = 0;

        // now calculate the speed from estimated end time and total size
        // funny how chrome only gives us a time whereas KJob operates on speed
        // and calculates the time this way :)

        const QDateTime endTime = QDateTime::fromString(it->toString(), Qt::ISODate);
        if (endTime.isValid()) {
            const QDateTime now = QDateTime::currentDateTimeUtc();

            qulonglong remainingBytes = totalAmount(Bytes) - processedAmount(Bytes);
            quint64 remainingTime = now.secsTo(endTime);

            if (remainingTime > 0) {
                speed = remainingBytes / remainingTime;
            }
        }

        emitSpeed(speed);
    }

    // TODO use the estimatedEndTime to calculate transfer speed

    it = payload.constFind(QStringLiteral("state"));
    if (it != end) {
        const QString status = it->toString();
        if (status == QLatin1String("in_progress")) {

        } else if (status == QLatin1String("interrupted")) {

            const QString &error = payload.value(QStringLiteral("error")).toString();

            if (error == QLatin1String("USER_CANCELED")
                || error == QLatin1String("USER_SHUTDOWN")) {
                setError(KIO::ERR_USER_CANCELED); // will keep Notification applet from showing a "finished"/error message
                emitResult();
                return;
            }

            // value is a QVariant so we can be lazy and support both KIO errors and custom test
            // if QVariant is an int: use that as KIO error
            // if QVariant is a QString: set UserError and message
            static const QHash<QString, QString> errors {
                // for a list of these error codes *and their meaning* instead of looking at browser
                // extension docs, check out Chromium's source code: download_interrupt_reason_values.h
                {QStringLiteral("FILE_ACCESS_DENIED"), i18n("Access denied.")}, // KIO::ERR_ACCESS_DENIED
                {QStringLiteral("FILE_NO_SPACE"), i18n("Insufficient free space.")}, // KIO::ERR_DISK_FULL
                {QStringLiteral("FILE_NAME_TOO_LONG"), i18n("The file name you have chosen is too long.")},
                {QStringLiteral("FILE_TOO_LARGE"), i18n("The file is too large to be downloaded.")},
                // haha
                {QStringLiteral("FILE_VIRUS_INFECTED"), i18n("The file possibly contains malicious contents.")},
                {QStringLiteral("FILE_TRANSIENT_ERROR"), i18n("A temporary error has occurred. Please try again later.")},

                {QStringLiteral("NETWORK_FAILED"), i18n("A network error has occurred.")},
                {QStringLiteral("NETWORK_TIMEOUT"), i18n("The network operation timed out.")}, // TODO something less geeky
                {QStringLiteral("NETWORK_DISCONNECTED"), i18n("The network connection has been lost.")},
                {QStringLiteral("NETWORK_SERVER_DOWN"), i18n("The server is no longer reachable.")},

                {QStringLiteral("SERVER_FAILED"), i18n("A server error has occurred.")},
                // chromium code says "internal use" and this is really not something the user should see
                // SERVER_NO_RANGE"
                // SERVER_PRECONDITION
                {QStringLiteral("SERVER_BAD_CONTENT"), i18n("The server does not have the requested data.")},

                {QStringLiteral("CRASH"), i18n("The browser application closed unexpectedly.")}
            };


            const QString &errorValue = errors.value(error);
            if (errorValue.isEmpty()) { // unknown error
                setError(KIO::ERR_UNKNOWN);
                setErrorText(i18n("An unknown error occurred while downloading."));
                emitResult();
                return;
            }

            // KIO::Error doesn't have a UserDefined one, let's just use magic numbers then
            // TODO at least set the KIO::Errors that we do have
            setError(1000);
            setErrorText(errorValue);

            emitResult();

            return;
        } else if (status == QLatin1String("complete")) {
            emitResult();
            return;
        }
    }

    if (descriptionDirty) {
        updateDescription();
    }
}

void DownloadJob::updateDescription()
{
    description(this, i18nc("Job heading, like 'Copying'", "Downloading"),
        qMakePair<QString, QString>(i18nc("The URL being downloaded", "Source"), (m_finalUrl.isValid() ? m_finalUrl : m_url).toDisplayString()),
        qMakePair<QString, QString>(i18nc("The location being downloaded to", "Destination"), m_destination.toLocalFile())
    );
}
