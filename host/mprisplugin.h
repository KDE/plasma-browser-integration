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

    // Player
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
    QString identity() const;
    QString desktopEntry() const;

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

private:
    void emitPropertyChange(const QDBusAbstractAdaptor *interface, const char *propertyName);

    bool registerService();
    bool unregisterService();

    void setPlaybackStatus(const QString &playbackStatus);
    void setLength(quint64 length);

    QDBusAbstractAdaptor *m_root;
    QDBusAbstractAdaptor *m_player;

    QString m_playbackStatus;

    QString m_pageTitle;
    QUrl m_url;

    QString m_title;
    QString m_artist;
    QUrl m_artworkUrl;

    quint64 m_length = 0;

};
