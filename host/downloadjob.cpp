/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "downloadjob.h"
#include "settings.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QJsonObject>

#include <KActivities/ResourceInstance>
#include <KFileMetaData/UserMetaData>
#include <KLocalizedString>
#include <KUiServerV2JobTracker>

#include <KIO/Global>

DownloadJob::DownloadJob()
    : KJob()
{
    // Tell KJobTracker to show the job right away so that we get a "finished"
    // notification even for tiny downloads
    setProperty("immediateProgressReporting", true);

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
    Q_EMIT killRequested();
    // TODO what if the user kills us from notification area while the
    // "Save As" prompt is still open?
    return true;
}

bool DownloadJob::doSuspend()
{
    Q_EMIT suspendRequested();
    return true;
}

bool DownloadJob::doResume()
{
    Q_EMIT resumeRequested();
    return true;
}

QUrl DownloadJob::originUrl() const
{
    QUrl url = (m_finalUrl.isValid() ? m_finalUrl : m_url);

    if (m_referrer.isValid()
            && (url.scheme() == QLatin1String("blob")
                || url.scheme() == QLatin1String("data"))) {
        url = m_referrer;
    }

    return url;
}

void DownloadJob::update(const QJsonObject &payload)
{
    auto end = payload.constEnd();

    bool descriptionDirty = false;

    const QUrl oldOriginUrl = originUrl();

    auto it = payload.constFind(QStringLiteral("url"));
    if (it != end) {
        m_url = QUrl(it->toString());
    }

    it = payload.constFind(QStringLiteral("finalUrl"));
    if (it != end) {
        m_finalUrl = QUrl(it->toString());
    }

    it = payload.constFind(QStringLiteral("referrer"));
    if (it != end) {
        m_referrer = QUrl(it->toString());
    }

    descriptionDirty = (originUrl() != oldOriginUrl);

    it = payload.constFind(QStringLiteral("filename"));
    if (it != end) {
        m_fileName = it->toString();

        const QUrl destination = QUrl::fromLocalFile(it->toString());

        setProperty("destUrl", destination.toString(QUrl::RemoveFilename | QUrl::StripTrailingSlash));

        if (m_destination != destination) {
            m_destination = destination;
            descriptionDirty = true;
        }
    }

    it = payload.constFind(QStringLiteral("mime"));
    if (it != end) {
        m_mimeType = it->toString();
    }

    it = payload.constFind(QStringLiteral("danger"));
    if (it != end) {
        const QString danger = it->toString();
        if (danger == QLatin1String("accepted")) {
            // Clears previous danger message
            infoMessage(this, QString());
        } else if (danger != QLatin1String("safe")) {
            infoMessage(this, i18n("This type of file can harm your computer. If you want to keep it, accept this download from the browser window."));
        }
    }

    it = payload.constFind(QStringLiteral("incognito"));
    if (it != end) {
        m_incognito = it->toBool();
    }

    it = payload.constFind(QStringLiteral("totalBytes"));
    if (it != end) {
        const qlonglong totalAmount = it->toDouble();
        if (totalAmount > -1) {
            setTotalAmount(Bytes, totalAmount);
        }
    }

    const auto oldBytesReceived = m_bytesReceived;
    it = payload.constFind(QStringLiteral("bytesReceived"));
    if (it != end) {
        m_bytesReceived = it->toDouble();
        setProcessedAmount(Bytes, m_bytesReceived);
    }

    setTotalAmount(Files, 1);

    it = payload.constFind(QStringLiteral("paused"));
    if (it != end) {
        const bool paused = it->toBool();

        if (paused) {
            suspend();
        } else {
            resume();
        }
    }

    bool speedValid = false;
    qulonglong speed = 0;
    it = payload.constFind(QStringLiteral("estimatedEndTime"));
    if (it != end) {
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
                speedValid = true;
                m_fallbackSpeedTimer.invalidate();
            }
        }
    }

    if (!speedValid) {
        if (!m_fallbackSpeedTimer.isValid()) {
            m_fallbackSpeedTimer.start();
        }

        // When download size isn't known, we don't get estimatedEndTime but there's
        // also no dedicated speed field, so we'll have to calculate that ourself now
        if (m_fallbackSpeedTimer.hasExpired(990)) { // not exactly 1000ms to account for some fuzziness
            const auto deltaBytes = m_bytesReceived - oldBytesReceived;
            speed = (deltaBytes * 1000) / m_fallbackSpeedTimer.elapsed();
            speedValid = true;
            m_fallbackSpeedTimer.start();
        }
    }

    if (speedValid) {
        emitSpeed(speed);
    }

    if (descriptionDirty) {
        updateDescription();
    }

    const QString error = payload.value(QStringLiteral("error")).toString();
    if (!error.isEmpty()) {
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

            {QStringLiteral("CRASH"), i18n("The browser application closed unexpectedly.")},
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
    }

    it = payload.constFind(QStringLiteral("state"));
    if (it != end) {
        const QString state = it->toString();

        // We ignore "interrupted" state and only cancel if we get supplied an "error"
        if (state == QLatin1String("complete")) {
            setError(KJob::NoError);
            setProcessedAmount(KJob::Files, 1);

            // Add to recent document
            addToRecentDocuments();

            // Write origin url into extended file attributes
            saveOriginUrl();

            emitResult();
            return;
        }
    }
}

void DownloadJob::updateDescription()
{
    description(this, i18nc("Job heading, like 'Copying'", "Downloading"),
        qMakePair<QString, QString>(i18nc("The URL being downloaded", "Source"), originUrl().toDisplayString()),
        qMakePair<QString, QString>(i18nc("The location being downloaded to", "Destination"), m_destination.toLocalFile())
    );
}

void DownloadJob::addToRecentDocuments()
{
    if (m_incognito || m_fileName.isEmpty()) {
        return;
    }

    const QJsonObject settings = Settings::self().settingsForPlugin(QStringLiteral("downloads"));

    const bool enabled = settings.value(QStringLiteral("addToRecentDocuments")).toBool();
    if (!enabled) {
        return;
    }

    KActivities::ResourceInstance::notifyAccessed(QUrl::fromLocalFile(m_fileName), qApp->desktopFileName());
}

void DownloadJob::saveOriginUrl()
{
    QUrl url = originUrl();

    if (m_incognito
        || !url.isValid()
        // Blob URLs are dynamically created through JavaScript and cannot be accessed from the outside
        || url.scheme() == QLatin1String("blob")
        // Data URLs contain the actual data of the file we just downloaded anyway
        || url.scheme() == QLatin1String("data")) {
        return;
    }

    const QJsonObject settings = Settings::self().settingsForPlugin(QStringLiteral("downloads"));

    const bool saveOriginUrl = settings.value(QStringLiteral("saveOriginUrl")).toBool();
    if (!saveOriginUrl) {
        return;
    }

    KFileMetaData::UserMetaData md(m_fileName);

    url.setPassword(QString());

    md.setOriginUrl(url);
}
