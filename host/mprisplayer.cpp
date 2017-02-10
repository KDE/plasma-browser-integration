#include "mprisplayer.h"

#include <QDBusConnection>

#include <QDebug>

MPrisPlayer::MPrisPlayer(QObject *parent) : QObject(parent)
{
    /*if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/mpris/MediaPlayer2"),
                                                      QStringLiteral("org.mpris.MediaPlayer2.Player"),
                                                      this,
                                                      QDBusConnection::ExportAllProperties
                                                      | QDBusConnection::ExportAllSlots
                                                      | QDBusConnection::ExportAllSignals)) {
        qWarning() << "failed to setup player";
        return;
    }*/

    //QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.plasma-chrome-integration");
}

MPrisPlayer::~MPrisPlayer() = default;

QString MPrisPlayer::playbackStatus() const
{
    return "";
}

QVariantMap MPrisPlayer::metaData() const
{
    return QVariantMap();
}

bool MPrisPlayer::canPlay() const
{
    return playbackStatus() != "Playing";
}

bool MPrisPlayer::canPause() const
{
    return playbackStatus() == "Playing";
}

void MPrisPlayer::Pause()
{

}

void MPrisPlayer::PlayPause()
{

}

void MPrisPlayer::Play()
{

}
