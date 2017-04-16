#include "mprisplugin.h"

#include <QDBusConnection>
#include <QDBusObjectPath>

#include "mprisroot.h"
#include "mprisplayer.h"

#include <QDebug>
#include <QImageReader>

static const QString s_serviceName = QStringLiteral("org.mpris.MediaPlayer2.plasma-chrome-integration");

MPrisPlugin::MPrisPlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("mpris"), 1, parent)
    , m_root(new MPrisRoot(this))
    , m_player(new MPrisPlayer(this))
{

    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this)) {
        qWarning() << "Failed to register MPris object";
        return;
    }
}

// TODO this can surely be done in a much beter way with introspection and what not
void MPrisPlugin::emitPropertyChange(const QDBusAbstractAdaptor *interface, const char *propertyName)
{
    // TODO don't assume it's index 0
    // TODO figure out encoding encoding
    const QString interfaceName = QString::fromUtf8(interface->metaObject()->classInfo(0).value());

    QDBusMessage signal = QDBusMessage::createSignal(
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );

    QMetaProperty prop = metaObject()->property(metaObject()->indexOfProperty(propertyName));

    signal.setArguments({
        interfaceName,
        QVariantMap{ // updated
            {QString::fromUtf8(prop.name()), prop.read(this)}
        },
        QStringList() // invalidated
    });

    QDBusConnection::sessionBus().send(signal);

    //qDebug() << "emitted change on iface" << interfaceName << "for prop" << propertyName << "which was" << signal;
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
        setPlaybackStatus(QStringLiteral("Stopped")); // just in case
        m_canGoNext = false;
        m_canGoPrevious = false;
        m_pageTitle.clear();
        m_url.clear();
        m_title.clear();
        m_artist.clear();
        m_artworkUrl.clear();
        m_length = 0;
        m_position = 0;
    } else if (event == QLatin1String("playing")) {
        setPlaybackStatus(QStringLiteral("Playing"));
        m_pageTitle = data.value(QStringLiteral("tabTitle")).toString();
        m_url = QUrl(data.value(QStringLiteral("url")).toString());

        const qreal length = data.value(QStringLiteral("duration")).toDouble();
        // <video> duration is in seconds, mpris uses microseconds
        setLength(length * 1000 * 1000);

        const qreal position = data.value(QStringLiteral("currentTime")).toDouble();
        setPosition(position * 1000 * 1000);

        processMetadata(data.value(QStringLiteral("metadata")).toObject()); // also emits metadataChanged signal
        processCallbacks(data.value(QStringLiteral("callbacks")).toArray());

        registerService();
    } else if (event == QLatin1String("paused")) {
        setPlaybackStatus(QStringLiteral("Paused"));
    } else if (event == QLatin1String("waiting")) {
        // unfortunately MPris doesn't have a "Buffering" playback state
        // set it to Paused when waiting for the player to avoid the seek slider
        // moving while we're not actually playing something
        // (we don't get an explicit "paused" signal)
        setPlaybackStatus(QStringLiteral("Paused"));
    } else if (event == QLatin1String("canplay")) {
        // opposite of "waiting", only forwarded by our extension when
        // canplay is emitted with the player *not* paused
        setPlaybackStatus(QStringLiteral("Playing"));
    } else if (event == QLatin1String("duration")) {
        const qreal length = data.value(QStringLiteral("duration")).toDouble();

        // <video> duration is in seconds, mpris uses microseconds
        setLength(length * 1000 * 1000);
    } else if (event == QLatin1String("timeupdate")) {
        // not signalling to avoid excess dbus traffic
        // media controller asks for this property once when it opens
        m_position = data.value(QStringLiteral("currentTime")).toDouble() * 1000 * 1000;
    } else if (event == QLatin1String("seeked")) {
        // seeked is explicit user interaction, signal a change on dbus
        const qreal position = data.value(QStringLiteral("currentTime")).toDouble();
        setPosition(position * 1000 * 1000);
    } else if (event == QLatin1String("metadata")) {
        processMetadata(data.value(QStringLiteral("metadata")).toObject());
    } else if (event == QLatin1String("callbacks")) {
        processCallbacks(data.value(QStringLiteral("callbacks")).toArray());
    } else {
        qWarning() << "Don't know how to handle mpris event" << event;
    }
}

QString MPrisPlugin::identity() const
{
    return QStringLiteral("Google Chrome"); // TODO return correct browser
}

QString MPrisPlugin::desktopEntry() const
{
    return QStringLiteral("google-chrome"); // TODO return correct browser
}

bool MPrisPlugin::canRaise() const
{
    return true; // really?
}

bool MPrisPlugin::canGoNext() const
{
    return m_canGoNext;
}

bool MPrisPlugin::canGoPrevious() const
{
    return m_canGoPrevious;
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

qlonglong MPrisPlugin::position() const
{
    return m_position;
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

    // HACK this is needed or else SetPosition won't do anything
    // TODO use something more sensible, e.g. at least have the tab id with the player in there or so
    metadata.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/kde/plasma/browser_integration/1337"))));

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
        //emit playbackStatusChanged();

        emitPropertyChange(m_player, "PlaybackStatus");
    }
}

void MPrisPlugin::setLength(qlonglong length)
{
    if (m_length != length) {
        m_length = length;

        //emit metadataChanged();

        emitPropertyChange(m_player, "Metadata");
        emitPropertyChange(m_player, "CanSeek");
    }
}

void MPrisPlugin::setPosition(qlonglong position)
{
    if (m_position != position) {
        m_position = position;

        emitPropertyChange(m_player, "Position");
    }
}

void MPrisPlugin::processMetadata(const QJsonObject &data)
{
    m_title = data.value(QStringLiteral("title")).toString();
    m_artist = data.value(QStringLiteral("artist")).toString();

    // for simplicity we just use the biggest artwork it offers, perhaps we could limit it to some extent
    // TODO download/cache artwork somewhere
    QSize biggest;
    QUrl artworkUrl;
    const QJsonArray &artwork = data.value(QStringLiteral("artwork")).toArray();

    const auto &supportedImageMimes = QImageReader::supportedMimeTypes();

    for (auto it = artwork.constBegin(), end = artwork.constEnd(); it != end; ++it) {
        const QJsonObject &item = it->toObject();

        const QUrl url = QUrl(item.value(QStringLiteral("src")).toString());
        if (!url.isValid()) {
            continue;
        }

        // why is this named "sizes" when it's just a string and the examples don't mention how one could specify multiple?
        // also, how on Earth could a single image src have multiple sizes? ...
        // spec says this is a space-separated list of sizes for ... some reason
        const QString sizeString = item.value(QStringLiteral("sizes")).toString();
        QSize actualSize;

        // now parse the size...
        const auto &sizeParts = sizeString.splitRef(QLatin1Char('x'));
        if (sizeParts.count() == 2) {
            const int width = sizeParts.first().toInt();
            const int height = sizeParts.last().toInt();
            if (width <= 0 || height <= 0) {
                continue;
            }

            actualSize = QSize(width, height);
        }

        const QString type = item.value(QStringLiteral("type")).toString();
        if (!type.isEmpty() && !supportedImageMimes.contains(type.toUtf8())) {
            continue;
        }

        if (!biggest.isValid() || (actualSize.width() >= biggest.width() && actualSize.height() >= biggest.height())) {
            artworkUrl = url;
            biggest = actualSize;
        }
    }

    m_artworkUrl = artworkUrl;

    emitPropertyChange(m_player, "Metadata");
}

void MPrisPlugin::processCallbacks(const QJsonArray &data)
{
    const bool canGoNext = data.contains(QLatin1String("nexttrack"));
    if (m_canGoNext != canGoNext) {
        m_canGoNext = canGoNext;
        emitPropertyChange(m_player, "CanGoNext");
    }

    const bool canGoPrevious = data.contains(QLatin1String("previoustrack"));
    if (m_canGoPrevious != canGoPrevious) {
        m_canGoPrevious = canGoPrevious;
        emitPropertyChange(m_player, "CanGoPrevious");
    }
}

void MPrisPlugin::Raise()
{
    sendData(QStringLiteral("raise"));
}

void MPrisPlugin::Quit()
{

}

void MPrisPlugin::Next()
{
    sendData(QStringLiteral("next"));
}

void MPrisPlugin::Previous()
{
    sendData(QStringLiteral("previous"));
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
    Q_UNUSED(path); // TODO use?

    sendData(QStringLiteral("setPosition"), {
        {QStringLiteral("position"), position / 1000.0 / 1000.0
    } });
}

void MPrisPlugin::OpenUri(const QString &uri)
{
    Q_UNUSED(uri);
}
