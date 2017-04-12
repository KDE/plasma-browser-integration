#pragma once

#include "abstractbrowserplugin.h"

#include <QString>
#include <QUrl>

class QDBusObjectPath;

class MPrisPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT

    Q_PROPERTY(bool CanControl READ canControl NOTIFY canControlChanged)
    Q_PROPERTY(bool CanPause READ canPause NOTIFY playbackStatusChanged)
    Q_PROPERTY(bool CanPlay READ canPlay NOTIFY playbackStatusChanged)
    Q_PROPERTY(bool CanSeek READ canSeek NOTIFY canSeekChanged)
    Q_PROPERTY(double MinimumRate READ minimumRate NOTIFY minimumRateChanged)
    Q_PROPERTY(double MaximumRate READ maximumRate NOTIFY maximumRateChanged)
    // TODO Volume
    // TODO LoopStatus
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus NOTIFY playbackStatusChanged)
    Q_PROPERTY(QVariantMap Metadata READ metadata NOTIFY metadataChanged)

public:
    MPrisPlugin(QObject *parent);

    void handleData(const QString &event, const QJsonObject &data) override;

    // mpris properties ____________

    // Root

    // Player
    bool canControl() const;
    bool canPause() const;
    bool canPlay() const;
    bool canSeek() const;

    double minimumRate() const;
    double maximumRate() const;

    QString playbackStatus() const;
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
    void minimumRateChanged();
    void maximumRateChanged();
    void metadataChanged();

private slots:
    void onPropertyChanged();

private:
    bool registerService();
    bool unregisterService();

    void setPlaybackStatus(const QString &playbackStatus);
    void setLength(quint64 length);

    QString m_playbackStatus;
    QString m_title;
    QUrl m_url;
    quint64 m_length = 0;

};
