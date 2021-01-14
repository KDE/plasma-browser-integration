/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include "abstractbrowserplugin.h"

#include <QHash>
#include <QString>
#include <QTimer>
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
    Q_PROPERTY(bool Fullscreen READ fullscreen WRITE setFullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool CanSetFullscreen READ canSetFullscreen NOTIFY canSetFullscreenChanged)

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
    explicit MPrisPlugin(QObject *parent);

    bool onUnload() override;

    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

    // mpris properties ____________

    // Root
    QString identity() const;
    QString desktopEntry() const;
    bool canRaise() const;

    bool fullscreen() const;
    void setFullscreen(bool fullscreen);
    bool canSetFullscreen() const;

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

Q_SIGNALS:
    void fullscreenChanged();
    void canSetFullscreenChanged();
    void canControlChanged();
    void playbackStatusChanged();
    void canSeekChanged();
    void playbackRateChanged();
    void minimumRateChanged();
    void maximumRateChanged();
    void metadataChanged();

private:
    void emitPropertyChange(const QDBusAbstractAdaptor *interface, const char *propertyName);
    void sendPropertyChanges();

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

    QTimer m_propertyChangeSignalTimer;
    // interface - <property name, value>
    QHash<QString, QVariantMap> m_pendingPropertyChanges;

    QString m_serviceName;

    bool m_fullscreen = false;
    bool m_canSetFullscreen = false;

    QString m_playbackStatus;
    QString m_loopStatus;
    QHash<QString, bool> m_possibleLoopStatus;

    bool m_canGoNext = false;
    bool m_canGoPrevious = false;

    QString m_pageTitle;
    QString m_tabTitle;

    QUrl m_url;
    QUrl m_mediaSrc;

    QString m_title;
    QString m_artist;
    QString m_album;
    QUrl m_posterUrl;
    QUrl m_artworkUrl;

    qreal m_volume = 1.0;
    bool m_muted = false;

    qlonglong m_length = 0;
    qlonglong m_position = 0;
    qreal m_playbackRate = 1.0;

};
