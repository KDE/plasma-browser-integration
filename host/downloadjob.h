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
