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

#include "abstractbrowserplugin.h"

#include <QString>
#include <QUrl>

class QDBusObjectPath;
class QDBusAbstractAdaptor;

class MPrisPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT

    // Root
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(bool CanRaise READ canRaise)

    // Player
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanControl READ canControl NOTIFY canControlChanged)
    Q_PROPERTY(bool CanPause READ canPause NOTIFY playbackStatusChanged)
    Q_PROPERTY(bool CanPlay READ canPlay NOTIFY playbackStatusChanged)
    Q_PROPERTY(bool CanSeek READ canSeek NOTIFY canSeekChanged)
    Q_PROPERTY(qreal Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double Rate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(double MinimumRate READ minimumRate NOTIFY minimumRateChanged)
    Q_PROPERTY(double MaximumRate READ maximumRate NOTIFY maximumRateChanged)
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus NOTIFY playbackStatusChanged)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(QVariantMap Metadata READ metadata NOTIFY metadataChanged)

public:
    MPrisPlugin(QObject *parent);

    bool onUnload() override;

    void handleData(const QString &event, const QJsonObject &data) override;

    // mpris properties ____________

    // Root
    QString identity() const;
    QString desktopEntry() const;
    bool canRaise() const;

    // Player
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canControl() const;
    bool canPause() const;
    bool canPlay() const;
    bool canSeek() const;

    qreal volume() const;
    void setVolume(qreal volume);

    qlonglong position() const;

    double playbackRate() const;
    void setPlaybackRate(double playbackRate);

    double minimumRate() const;
    double maximumRate() const;

    QString playbackStatus() const;

    QString loopStatus() const;
    void setLoopStatus(const QString &loopStatus);

    QVariantMap metadata() const;

    // dbus-exported methods ________

    // Root
    void Raise();
    void Quit();

    // Player
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &path, qlonglong position);
    void OpenUri(const QString &uri);

signals:
    void canControlChanged();
    void playbackStatusChanged();
    void canSeekChanged();
    void playbackRateChanged();
    void minimumRateChanged();
    void maximumRateChanged();
    void metadataChanged();

private:
    void emitPropertyChange(const QDBusAbstractAdaptor *interface, const char *propertyName);

    bool registerService();
    bool unregisterService();

    void setPlaybackStatus(const QString &playbackStatus);
    void setLength(qlonglong length);
    void setPosition(qlonglong position);
    void processMetadata(const QJsonObject &data);
    void processCallbacks(const QJsonArray &data);

    QString effectiveTitle() const;

    QDBusAbstractAdaptor *m_root;
    QDBusAbstractAdaptor *m_player;

    QString m_serviceName;

    QString m_playbackStatus;
    QString m_loopStatus;
    QHash<QString, bool> m_possibleLoopStatus;

    bool m_canGoNext = false;
    bool m_canGoPrevious = false;

    QString m_pageTitle;
    QUrl m_url;

    QString m_title;
    QString m_artist;
    QString m_album;
    QUrl m_artworkUrl;

    qreal m_volume = 1.0;

    qlonglong m_length = 0;
    qlonglong m_position = 0;
    qreal m_playbackRate = 1.0;

};
