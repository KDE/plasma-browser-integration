#include "mprisplugin.h"

#include <QDBusConnection>
#include <QDBusObjectPath>

#include "mprisroot.h"
#include "mprisplayer.h"

#include <QDebug>

static const QString s_serviceName = QStringLiteral("org.mpris.MediaPlayer2.plasma-chrome-integration");

MPrisPlugin::MPrisPlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("mpris"), 1, parent)
{
    new MPrisRoot(this);
    new MPrisPlayer(this);

    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this)) {
        qWarning() << "Failed to register MPris object";
        return;
    }

    // TODO use some introspection and QSignalMapper magic to auto-emit the correct PropertyChanged signal :D
    /*QMetaMethod changedHandler = metaObject()->method(metaObject()->indexOfSlot("onPropertyChanged()"));

    for (int i = metaObject()->propertyOffset(); i < metaObject()->propertyCount(); ++i) {
        QMetaProperty prop = metaObject()->property(i);
        if (prop.hasNotifySignal()) {
            connect(this, prop.notifySignal(), this, changedHandler);
        }
    }*/
}

void MPrisPlugin::onPropertyChanged()
{
    /*// FIXME emit the correct one and not just all the things

    QDBusMessage signal = QDBusMessage::createSignal(
        QStringLiteral("/org/mpris/MediaPlayer"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );

    signal.setArguments({
        QStringLiteral("org.mpris.MediaPlayer2.Player"),
        QVariantMap{
            {QStringLiteral("PlaybackStatus"), playbackStatus()}
        }
    });

    QDBusConnection::sessionBus().send(signal);*/
}

bool MPrisPlugin::registerService()
{
    // TODO append pid
    return QDBusConnection::sessionBus().registerService(s_serviceName);
}

bool MPrisPlugin::unregisterService()
{
    // TODO append pid
    return QDBusConnection::sessionBus().unregisterService(s_serviceName);
}

void MPrisPlugin::handleData(const QString &event, const QJsonObject &data)
{
    if (event == QLatin1String("gone")) {
        unregisterService();
    } else if (event == QLatin1String("playing")) {
        setPlaybackStatus(QStringLiteral("Playing"));
        m_pageTitle = data.value(QStringLiteral("title")).toString();
        m_url = QUrl(data.value(QStringLiteral("url")).toString());
        emit metadataChanged();

        registerService();
    } else if (event == QLatin1String("paused")) {
        setPlaybackStatus(QStringLiteral("Paused"));
    } else if (event == QLatin1String("duration")) {
        const qreal length = data.value(QStringLiteral("duration")).toDouble();

        // <video> duration is in seconds, mpris uses microseconds
        setLength(length * 1000 * 1000);
    } else if (event == QLatin1String("metadata")) {
        m_title = data.value(QStringLiteral("title")).toString();
        m_artist = data.value(QStringLiteral("artist")).toString();

        // for simplicity we just use the biggest artwork it offers, perhaps we could limit it to some extent
        // TODO download/cache artwork somewhere
        QSize biggest;
        QUrl artworkUrl;
        const QJsonArray &artwork = data.value(QStringLiteral("artwork")).toArray();
        for (auto it = artwork.constBegin(), end = artwork.constEnd(); it != end; ++it) {
            const QJsonObject &item = it->toObject();

            const QUrl url = QUrl(item.value(QStringLiteral("src")).toString());
            if (!url.isValid()) {
                continue;
            }

            // why is this named "sizes" when it's just a string and the examples don't mention how one could specify multiple?
            // also, how on Earth could a single image src have multiple sizes? ...
            const QString sizeString = item.value(QStringLiteral("sizes")).toString();
            // now parse the size...
            const auto &sizeParts = sizeString.splitRef(QLatin1Char('x'));
            if (sizeParts.count() != 2) {
                continue;
            }

            const int width = sizeParts.first().toInt();
            const int height = sizeParts.last().toInt();
            if (width <= 0 || height <= 0) {
                continue;
            }

            const QSize actualSize(width, height);
            if (!biggest.isValid() || (actualSize.width() >= biggest.width() && actualSize.height() >= biggest.height())) {
                artworkUrl = url;
                biggest = actualSize;
            }
        }

        m_artworkUrl = artworkUrl;

        emit metadataChanged();
    } else {
        qWarning() << "Don't know how to handle mpris event" << event;
    }
}

bool MPrisPlugin::canControl() const
{
    return true; // really?
}

bool MPrisPlugin::canPause() const
{
    return canControl() && playbackStatus() == QLatin1String("Playing");
}

bool MPrisPlugin::canPlay() const
{
    return canControl() && playbackStatus() != QLatin1String("Paused");
}

bool MPrisPlugin::canSeek() const
{
    // TODO use player.seekable for determining whether we can seek?
    return m_length > 0;
}

qreal MPrisPlugin::minimumRate() const
{
    return 1; // TODO
}

qreal MPrisPlugin::maximumRate() const
{
    return 1;
}

// TODO volume

// TODO loop status

QString MPrisPlugin::playbackStatus() const
{
    return m_playbackStatus;
}

QVariantMap MPrisPlugin::metadata() const
{
    QVariantMap metadata;

    const QString &effectiveTitle = !m_title.isEmpty() ? m_title : m_pageTitle;
    if (!effectiveTitle.isEmpty()) {
        metadata.insert(QStringLiteral("xesam:title"), effectiveTitle);
    }

    if (m_url.isValid()) {
        metadata.insert(QStringLiteral("xesam:url"), m_url.toDisplayString());
    }
    if (m_length > 0) {
        metadata.insert(QStringLiteral("mpris:length"), m_length);
    }
    if (!m_artist.isEmpty()) {
        metadata.insert(QStringLiteral("xesam:artist"), m_artist);
    }
    if (m_artworkUrl.isValid()) {
        metadata.insert(QStringLiteral("mpris:artUrl"), m_artworkUrl.toDisplayString());
    }

    // TODO album name and stuff

    return metadata;
}

void MPrisPlugin::setPlaybackStatus(const QString &playbackStatus)
{
    if (m_playbackStatus != playbackStatus) {
        m_playbackStatus = playbackStatus;
        emit playbackStatusChanged();
    }
}

void MPrisPlugin::setLength(quint64 length)
{
    if (m_length != length) {
        m_length = length;

        emit metadataChanged();

        emit canSeekChanged(); // TODO
    }
}

void MPrisPlugin::Raise()
{

}

void MPrisPlugin::Quit()
{

}

void MPrisPlugin::Next()
{

}

void MPrisPlugin::Previous()
{

}

void MPrisPlugin::Pause()
{
    sendData(QStringLiteral("pause"));
}

void MPrisPlugin::PlayPause()
{
    sendData(QStringLiteral("playPause"));
}

void MPrisPlugin::Stop()
{
    sendData(QStringLiteral("stop"));
}

void MPrisPlugin::Play()
{
    sendData(QStringLiteral("play"));
}

void MPrisPlugin::Seek(qlonglong offset)
{
    Q_UNUSED(offset);
}

void MPrisPlugin::SetPosition(const QDBusObjectPath &path, qlonglong position)
{
    Q_UNUSED(path);
    Q_UNUSED(position);
}

void MPrisPlugin::OpenUri(const QString &uri)
{
    Q_UNUSED(uri);
}
