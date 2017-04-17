#include "downloadjob.h"
#include "settings.h"

#include <QJsonObject>

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

            if (error == QLatin1String("USER_CANCELED")) {
                // don't show an error, KIO::ERR_USER_CANCELED is 1 which the notification applet looks for
                setError(1);
            } else {
                setError(2); // TODO proper values
                setErrorText(error); // TODO nice text
            }

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
    description(this, "Downloading",
        qMakePair<QString, QString>("Source", m_url.toDisplayString()),
        qMakePair<QString, QString>("Destination", m_destination.toLocalFile())
    );
}
