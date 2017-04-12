#pragma once

#include <KJob>

#include <QUrl>

#include <QDateTime>

class DownloadJob : public KJob
{
    Q_OBJECT

public:
    DownloadJob(int id);

    enum class State {
        None,
        InProgress,
        Interrupted,
        Complete
    };

    void start() override;

    void update(const QJsonObject &payload);

signals:
    void killRequested();
    void suspendRequested();
    void resumeRequested();

private slots:
    void doStart();

protected:
    bool doKill() override;
    bool doSuspend() override;
    bool doResume() override;

private:
    void updateDescription();

    int m_id = -1;

    QUrl m_url;
    QUrl m_finalUrl;

    QUrl m_destination;

    QString m_fileName;

    QString m_mimeType;

    QDateTime m_startTime;
    QDateTime m_endTime;

};
