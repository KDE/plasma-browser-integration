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
        m_title = data.value(QStringLiteral("title")).toString();
        m_url = QUrl(data.value(QStringLiteral("url")).toString());
        // TODO proper metadata
        emit metadataChanged();

        registerService();
    } else if (event == QLatin1String("paused")) {
        setPlaybackStatus(QStringLiteral("Paused"));
    } else if (event == QLatin1String("duration")) {
        const qreal length = data.value(QStringLiteral("duration")).toDouble();

        // <video> duration is in seconds, mpris uses microseconds
        setLength(length * 1000 * 1000);
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
    // TODO get proper metadata, possibly from the new media sessions API?
    if (!m_title.isEmpty()) {
        metadata.insert(QStringLiteral("xesam:title"), m_title);
    }
    if (m_url.isValid()) {
        metadata.insert(QStringLiteral("xesam:url"), m_url.toDisplayString());
    }
    if (m_length > 0) {
        metadata.insert(QStringLiteral("mpris:length"), m_length);
    }
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
