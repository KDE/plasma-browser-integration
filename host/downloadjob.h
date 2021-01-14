/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include <KJob>

#include <QElapsedTimer>
#include <QUrl>

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

Q_SIGNALS:
    void killRequested();
    void suspendRequested();
    void resumeRequested();

private Q_SLOTS:
    void doStart();

protected:
    bool doKill() override;
    bool doSuspend() override;
    bool doResume() override;

private:
    void updateDescription();
    void addToRecentDocuments();
    void saveOriginUrl();

    QUrl originUrl() const;

    int m_id = -1;

    QUrl m_url;
    QUrl m_finalUrl;
    QUrl m_referrer;

    QUrl m_destination;

    QString m_fileName;

    QString m_mimeType;

    qulonglong m_bytesReceived = 0;
    QElapsedTimer m_fallbackSpeedTimer;

    // In doubt, assume incognito
    bool m_incognito = true;

};
