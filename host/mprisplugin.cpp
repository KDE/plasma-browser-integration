/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "mprisplugin.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QGuiApplication>
#include <QImageReader>

#include "mprisplayer.h"
#include "mprisroot.h"

#include <unistd.h> // getppid

static const QString s_serviceName = QStringLiteral("org.mpris.MediaPlayer2.plasma-browser-integration");

MPrisPlugin::MPrisPlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("mpris"), 1, parent)
    , m_root(new MPrisRoot(this))
    , m_player(new MPrisPlayer(this))
    , m_playbackStatus(QStringLiteral("Stopped"))
    , m_loopStatus(QStringLiteral("None"))
{
    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this)) {
        qWarning() << "Failed to register MPris object";
        return;
    }

    m_propertyChangeSignalTimer.setInterval(0);
    m_propertyChangeSignalTimer.setSingleShot(true);
    connect(&m_propertyChangeSignalTimer, &QTimer::timeout, this, &MPrisPlugin::sendPropertyChanges);

    m_possibleLoopStatus = {
        {QStringLiteral("None"), false},
        {QStringLiteral("Track"), true},
        {QStringLiteral("Playlist"), true},
    };
}

bool MPrisPlugin::onUnload()
{
    unregisterService();
    return true;
}

// TODO this can surely be done in a much beter way with introspection and what not
void MPrisPlugin::emitPropertyChange(const QDBusAbstractAdaptor *interface, const char *propertyName)
{
    // TODO don't assume it's index 0
    // TODO figure out encoding encoding
    const QString interfaceName = QString::fromUtf8(interface->metaObject()->classInfo(0).value());

    const QMetaProperty prop = metaObject()->property(metaObject()->indexOfProperty(propertyName));

    const QString propertyNameString = QString::fromUtf8(prop.name());
    const QVariant value = prop.read(this);

    m_pendingPropertyChanges[interfaceName][propertyNameString] = value;

    if (!m_propertyChangeSignalTimer.isActive()) {
        m_propertyChangeSignalTimer.start();
    }
}

void MPrisPlugin::sendPropertyChanges()
{
    for (auto it = m_pendingPropertyChanges.constBegin(), end = m_pendingPropertyChanges.constEnd(); it != end; ++it) {
        const QString &interfaceName = it.key();

        const QVariantMap &changes = it.value();
        if (changes.isEmpty()) {
            continue;
        }

        QDBusMessage signal = QDBusMessage::createSignal(QStringLiteral("/org/mpris/MediaPlayer2"),
                                                         QStringLiteral("org.freedesktop.DBus.Properties"),
                                                         QStringLiteral("PropertiesChanged"));

        signal.setArguments({
            interfaceName,
            changes,
            QStringList(), // invalidated
        });

        QDBusConnection::sessionBus().send(signal);
    }

    m_pendingPropertyChanges.clear();
}

bool MPrisPlugin::registerService()
{
    QString serviceName = s_serviceName;

    if (QDBusConnection::sessionBus().registerService(serviceName)) {
        m_serviceName = serviceName;
        return true;
    }

    // now try appending PID in case multiple hosts are running
    serviceName.append(QLatin1String("-")).append(QString::number(QCoreApplication::applicationPid()));

    if (QDBusConnection::sessionBus().registerService(serviceName)) {
        m_serviceName = serviceName;
        return true;
    }

    m_serviceName.clear();
    return false;
}

bool MPrisPlugin::unregisterService()
{
    if (m_serviceName.isEmpty()) {
        return false;
    }
    return QDBusConnection::sessionBus().unregisterService(m_serviceName);
}

void MPrisPlugin::handleData(const QString &event, const QJsonObject &data)
{
    if (event == QLatin1String("gone")) {
        unregisterService();
        setPlaybackStatus(QStringLiteral("Stopped")); // just in case
        m_canGoNext = false;
        m_canGoPrevious = false;
        m_pageTitle.clear();
        m_tabTitle.clear();
        m_url.clear();
        m_mediaSrc.clear();
        m_title.clear();
        m_artist.clear();
        m_artworkUrl.clear();
        m_volume = 1.0;
        m_muted = false;
        m_length = 0;
        m_position = 0;
    } else if (event == QLatin1String("playing")) {
        setPlaybackStatus(QStringLiteral("Playing"));

        m_pageTitle = data.value(QLatin1String("pageTitle")).toString();
        m_tabTitle = data.value(QLatin1String("tabTitle")).toString();

        m_url = QUrl(data.value(QLatin1String("url")).toString());
        m_mediaSrc = QUrl(data.value(QLatin1String("mediaSrc")).toString());

        const QUrl posterUrl = QUrl(data.value(QLatin1String("poster")).toString());
        if (m_posterUrl != posterUrl) {
            m_posterUrl = posterUrl;
            emitPropertyChange(m_player, "Metadata");
        }

        const qreal oldVolume = volume();

        m_volume = data.value(QLatin1String("volume")).toDouble(1);
        m_muted = data.value(QLatin1String("muted")).toBool();

        if (volume() != oldVolume) {
            emitPropertyChange(m_player, "Volume");
        }

        const qreal length = data.value(QLatin1String("duration")).toDouble();
        // <video> duration is in seconds, mpris uses microseconds
        setLength(length * 1000 * 1000);

        const qreal position = data.value(QLatin1String("currentTime")).toDouble();
        setPosition(position * 1000 * 1000);

        const qreal playbackRate = data.value(QLatin1String("playbackRate")).toDouble(1);
        if (m_playbackRate != playbackRate) {
            m_playbackRate = playbackRate;
            emitPropertyChange(m_player, "Rate");
        }

        // check if we're already looping, that keeps us from forcefully
        // overwriting Playlist loop with Track loop
        const bool oldLoop = m_possibleLoopStatus.value(m_loopStatus);
        const bool loop = data.value(QLatin1String("loop")).toBool();

        if (loop != oldLoop) {
            setLoopStatus(loop ? QStringLiteral("Track") : QStringLiteral("None"));
        }

        const bool fullscreen = data.value(QLatin1String("fullscreen")).toBool();
        if (m_fullscreen != fullscreen) {
            m_fullscreen = fullscreen;
            emitPropertyChange(m_root, "Fullscreen");
        }

        const bool canSetFullscreen = data.value(QLatin1String("canSetFullscreen")).toBool();
        if (m_canSetFullscreen != canSetFullscreen) {
            m_canSetFullscreen = canSetFullscreen;
            emitPropertyChange(m_root, "CanSetFullscreen");
        }

        processMetadata(data.value(QLatin1String("metadata")).toObject()); // also emits metadataChanged signal
        processCallbacks(data.value(QLatin1String("callbacks")).toArray());

        registerService();
    } else if (event == QLatin1String("paused")) {
        setPlaybackStatus(QStringLiteral("Paused"));
    } else if (event == QLatin1String("stopped")) {
        setPlaybackStatus(QStringLiteral("Stopped"));
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
        const qreal length = data.value(QLatin1String("duration")).toDouble();

        // <video> duration is in seconds, mpris uses microseconds
        setLength(length * 1000 * 1000);
    } else if (event == QLatin1String("timeupdate")) {
        // not signalling to avoid excess dbus traffic
        // media controller asks for this property once when it opens
        m_position = data.value(QLatin1String("currentTime")).toDouble() * 1000 * 1000;
    } else if (event == QLatin1String("ratechange")) {
        m_playbackRate = data.value(QLatin1String("playbackRate")).toDouble(1);
        emitPropertyChange(m_player, "Rate");
    } else if (event == QLatin1String("seeking") || event == QLatin1String("seeked")) {
        // seeked is explicit user interaction, signal a change on dbus
        const qreal position = data.value(QLatin1String("currentTime")).toDouble();
        // FIXME actually invoke "Seeked" signal
        setPosition(position * 1000 * 1000);
    } else if (event == QLatin1String("volumechange")) {
        m_volume = data.value(QLatin1String("volume")).toDouble(1);
        m_muted = data.value(QLatin1String("muted")).toBool();
        emitPropertyChange(m_player, "Volume");
    } else if (event == QLatin1String("metadata")) {
        processMetadata(data.value(QLatin1String("metadata")).toObject());
    } else if (event == QLatin1String("callbacks")) {
        processCallbacks(data.value(QLatin1String("callbacks")).toArray());
    } else if (event == QLatin1String("titlechange")) {
        const QString oldTitle = effectiveTitle();
        m_pageTitle = data.value(QLatin1String("pageTitle")).toString();

        if (oldTitle != effectiveTitle()) {
            emitPropertyChange(m_player, "Metadata");
        }
    } else if (event == QLatin1String("fullscreenchange")) {
        const bool fullscreen = data.value(QLatin1String("fullscreen")).toBool();
        if (m_fullscreen != fullscreen) {
            m_fullscreen = fullscreen;
            emitPropertyChange(m_root, "Fullscreen");
        }
    } else {
        qWarning() << "Don't know how to handle mpris event" << event;
    }
}

QString MPrisPlugin::identity() const
{
    return QGuiApplication::applicationDisplayName();
}

QString MPrisPlugin::desktopEntry() const
{
    return QGuiApplication::desktopFileName();
}

bool MPrisPlugin::canRaise() const
{
    return true; // really?
}

bool MPrisPlugin::fullscreen() const
{
    return m_fullscreen;
}

void MPrisPlugin::setFullscreen(bool fullscreen)
{
    sendData(QStringLiteral("setFullscreen"),
             {
                 {QStringLiteral("fullscreen"), fullscreen},
             });
}

bool MPrisPlugin::canSetFullscreen() const
{
    return m_canSetFullscreen;
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
    return canControl();
}

bool MPrisPlugin::canPlay() const
{
    return canControl();
}

bool MPrisPlugin::canSeek() const
{
    // TODO use player.seekable for determining whether we can seek?
    return m_length > 0;
}

qreal MPrisPlugin::volume() const
{
    if (m_muted) {
        return 0.0;
    }
    return m_volume;
}

void MPrisPlugin::setVolume(qreal volume)
{
    if (volume < 0) {
        volume = 0.0;
    }

    sendData(QStringLiteral("setVolume"),
             {
                 {QStringLiteral("volume"), volume},
             });
}

qlonglong MPrisPlugin::position() const
{
    return m_position;
}

double MPrisPlugin::playbackRate() const
{
    return m_playbackRate;
}

void MPrisPlugin::setPlaybackRate(double playbackRate)
{
    if (playbackRate < minimumRate() || playbackRate > maximumRate()) {
        return;
    }

    sendData(QStringLiteral("setPlaybackRate"),
             {
                 {QStringLiteral("playbackRate"), playbackRate},
             });
}

double MPrisPlugin::minimumRate() const
{
    return 0.01; // don't let it stop
}

double MPrisPlugin::maximumRate() const
{
    return 32; // random
}

QString MPrisPlugin::playbackStatus() const
{
    return m_playbackStatus;
}

QString MPrisPlugin::loopStatus() const
{
    return m_loopStatus;
}

void MPrisPlugin::setLoopStatus(const QString &loopStatus)
{
    if (!m_possibleLoopStatus.contains(loopStatus)) {
        return;
    }

    sendData(QStringLiteral("setLoop"),
             {
                 {QStringLiteral("loop"), m_possibleLoopStatus.value(loopStatus)},
             });

    m_loopStatus = loopStatus;
    emitPropertyChange(m_player, "LoopStatus");
}

QString MPrisPlugin::effectiveTitle() const
{
    if (!m_title.isEmpty()) {
        return m_title;
    }
    if (!m_pageTitle.isEmpty()) {
        return m_pageTitle;
    }
    return m_tabTitle;
}

QVariantMap MPrisPlugin::metadata() const
{
    QVariantMap metadata;

    // HACK this is needed or else SetPosition won't do anything
    // TODO use something more sensible, e.g. at least have the tab id with the player in there or so
    metadata.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/kde/plasma/browser_integration/1337"))));

    // Task Manager matches the window to the player's PID but in our case
    // the browser window isn't owned by us
    metadata.insert(QStringLiteral("kde:pid"), getppid());

    const QString title = effectiveTitle();
    if (!title.isEmpty()) {
        metadata.insert(QStringLiteral("xesam:title"), title);
    }

    if (m_url.isValid()) {
        metadata.insert(QStringLiteral("xesam:url"), m_url.toDisplayString());
    }
    if (m_mediaSrc.isValid()) {
        metadata.insert(QStringLiteral("kde:mediaSrc"), m_mediaSrc.toDisplayString());
    }
    if (m_length > 0) {
        metadata.insert(QStringLiteral("mpris:length"), m_length);
    }
    // See https://searchfox.org/mozilla-central/rev/a3c18883ef9875ba4bb0cc2e7d6ba5a198aaf9bd/dom/media/mediasession/MediaMetadata.h#34
    // and
    // https://source.chromium.org/chromium/chromium/src/+/main:services/media_session/public/cpp/media_metadata.h;l=46;drc=098756533733ea50b2dcb1c40d9a9e18d49febbe
    // MediaMetadata.artist is of string type, but "xesam:artist" is of stringlist type
    if (!m_artist.isEmpty()) {
        metadata.insert(QStringLiteral("xesam:artist"), QStringList{m_artist});
    }

    QUrl artUrl = m_artworkUrl;
    if (!artUrl.isValid()) {
        artUrl = m_posterUrl;
    }
    if (artUrl.isValid()) {
        metadata.insert(QStringLiteral("mpris:artUrl"), artUrl.toDisplayString());
    }

    if (!m_album.isEmpty()) {
        metadata.insert(QStringLiteral("xesam:album"), m_album);
        // when we don't have artist information use the scheme+domain as "album" (that's what Chrome on Android does)
    } else if (m_artist.isEmpty() && m_url.isValid()) {
        metadata.insert(QStringLiteral("xesam:album"), m_url.toDisplayString(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment));
    }

    return metadata;
}

void MPrisPlugin::setPlaybackStatus(const QString &playbackStatus)
{
    if (m_playbackStatus != playbackStatus) {
        m_playbackStatus = playbackStatus;
        // emit playbackStatusChanged();

        emitPropertyChange(m_player, "PlaybackStatus");
        // these depend on playback status, so signal a change for these, too
        emitPropertyChange(m_player, "CanPlay");
        emitPropertyChange(m_player, "CanPause");
    }
}

void MPrisPlugin::setLength(qlonglong length)
{
    if (m_length != length) {
        const bool oldCanSeek = canSeek();

        m_length = length;
        emitPropertyChange(m_player, "Metadata");

        if (oldCanSeek != canSeek()) {
            emitPropertyChange(m_player, "CanSeek");
        }
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
    m_title = data.value(QLatin1String("title")).toString();
    m_artist = data.value(QLatin1String("artist")).toString();
    m_album = data.value(QLatin1String("album")).toString();

    // for simplicity we just use the biggest artwork it offers, perhaps we could limit it to some extent
    // TODO download/cache artwork somewhere
    QSize biggest;
    QUrl artworkUrl;
    const QJsonArray &artwork = data.value(QLatin1String("artwork")).toArray();

    const auto &supportedImageMimes = QImageReader::supportedMimeTypes();

    for (auto it = artwork.constBegin(), end = artwork.constEnd(); it != end; ++it) {
        const QJsonObject &item = it->toObject();

        const QUrl url = QUrl(item.value(QLatin1String("src")).toString());
        if (!url.isValid()) {
            continue;
        }

        // why is this named "sizes" when it's just a string and the examples don't mention how one could specify multiple?
        // also, how on Earth could a single image src have multiple sizes? ...
        // spec says this is a space-separated list of sizes for ... some reason
        const QString sizeString = item.value(QLatin1String("sizes")).toString();
        QSize actualSize;

        // now parse the size...
        const auto &sizeParts = QStringView(sizeString).split(QLatin1Char('x'));
        if (sizeParts.count() == 2) {
            const int width = sizeParts.first().toInt();
            const int height = sizeParts.last().toInt();
            actualSize = QSize(width, height);
        }

        const QString type = item.value(QLatin1String("type")).toString();
        if (!type.isEmpty() && !supportedImageMimes.contains(type.toUtf8())) {
            continue;
        }

        if (biggest.isEmpty() || (actualSize.width() >= biggest.width() && actualSize.height() >= biggest.height())) {
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
    if (!m_canGoNext) {
        return;
    }
    sendData(QStringLiteral("next"));
}

void MPrisPlugin::Previous()
{
    if (!m_canGoPrevious) {
        return;
    }
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
    auto newPosition = position() + offset;
    if (newPosition >= m_length) {
        Next();
        return;
    }

    if (newPosition < 0) {
        newPosition = 0;
    }
    SetPosition(QDBusObjectPath() /*unused*/, newPosition);
}

void MPrisPlugin::SetPosition(const QDBusObjectPath &path, qlonglong position)
{
    Q_UNUSED(path); // TODO use?

    if (position < 0 || position >= m_length) {
        return;
    }

    sendData(QStringLiteral("setPosition"), {{QStringLiteral("position"), position / 1000.0 / 1000.0}});
}

#include "moc_mprisplugin.cpp"
