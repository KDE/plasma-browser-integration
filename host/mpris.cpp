#include "mpris.h"
#include "mprisplayer.h"

#include <QDBusConnection>

#include <unistd.h>

MPris::MPris(QObject *parent) : QObject(parent)
{

    auto *player = new MPrisPlayer(this);
    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/mpris/MediaPlayer2"),
                                                      QStringLiteral("org.mpris.MediaPlayer2"),
                                                      this,
                                                      QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSlots
                                                      | QDBusConnection::ExportChildObjects)) {
        //std::cerr << "failed to register mpris interface";
        return;
    }


    QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.plasma-chrome-integration");

}

MPris::~MPris() = default;

bool MPris::canQuit() const
{
    return false;
}

bool MPris::canRaise() const
{
    return false;
}

bool MPris::hasTrackList() const
{
    return false;
}

QString MPris::identity() const
{
    return "google-chrome";
}

QString MPris::desktopEntry() const
{
    return "google-chrome";
}

QStringList MPris::supportedUriSchemes() const
{
    return {};
}

QStringList MPris::supportedMimeTypes() const
{
    return {};
}

void MPris::Raise()
{

}

void MPris::Quit()
{

}
