#include <QObject>

#include <QVariantMap>

class QDBusObjectPath;

class MPrisPlayer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")

    Q_PROPERTY(QString PlaybackStatus READ playbackStatus NOTIFY playbackStatusChanged)
    //Q_PROPERTY(QString LoopStatus )
    Q_PROPERTY(double Rate READ rate NOTIFY rateChanged)
    //Q_PROPERTY(bool Shuffle READ shuffle NOTIFY shuffleChanged)
    Q_PROPERTY(QVariantMap Metadata READ metaData NOTIFY metadataChanged)
    Q_PROPERTY(double Volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(qlonglong Position READ position NOTIFY positionChanged)
    Q_PROPERTY(double MinimumRate READ minimumRate NOTIFY minimumRateChanged)
    Q_PROPERTY(double MaximumRate READ maximumRate NOTIFY maximumRateChanged)
    Q_PROPERTY(bool CanGoNext READ canGoNext NOTIFY canGoNextChanged)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious NOTIFY canGoPreviousChanged)
    Q_PROPERTY(bool CanPlay READ canPlay NOTIFY canPlayChanged)
    Q_PROPERTY(bool CanPause READ canPause NOTIFY canPauseChanged)
    Q_PROPERTY(bool CanSeek READ canSeek NOTIFY canSeekChanged)
    Q_PROPERTY(bool CanControl READ canControl NOTIFY canControlChanged)

public:
    explicit MPrisPlayer(QObject *parent = nullptr);
    ~MPrisPlayer() override;

    QString playbackStatus() const;
    double rate() const { return 1.0; }
    QVariantMap metaData() const;
    double volume() const { return 1.0; }
    qlonglong position() const { return 0; }
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    bool canGoNext() const { return false; }
    bool canGoPrevious() const { return false; }
    bool canPlay() const;
    bool canPause() const;
    bool canSeek() const { return false; }
    bool canControl() const { return true; }

signals:
    void playbackStatusChanged();
    void rateChanged();
    void metadataChanged();
    void volumeChanged();
    void positionChanged();
    void minimumRateChanged();
    void maximumRateChanged();
    void canGoNextChanged();
    void canGoPreviousChanged();
    void canPlayChanged();
    void canPauseChanged();
    void canSeekChanged();
    void canControlChanged();

public slots:
    void Next() {}
    void Previous() {}
    void Pause();
    void PlayPause();
    void Stop() {}
    void Play();
    void Seek(qlonglong Offset) {}
    void SetPosition(const QDBusObjectPath &TrackId, qlonglong Position) {}
    void OpenUri(const QString &Uri) {}

};
