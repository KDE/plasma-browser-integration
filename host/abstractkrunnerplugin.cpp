/*
 *   Copyright (C) 2020 Kai Uwe Broulik <kde@broulik.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 3 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "abstractkrunnerplugin.h"

#include <QDBusConnection>
#include <QImage>

#include "krunner1adaptor.h"

AbstractKRunnerPlugin::AbstractKRunnerPlugin(const QString &objectPath,
                                             const QString &subsystemId,
                                             int protocolVersion,
                                             QObject *parent)
    : AbstractBrowserPlugin(subsystemId, protocolVersion, parent)
    , m_objectPath(objectPath)
{
    new Krunner1Adaptor(this);
    qDBusRegisterMetaType<RemoteMatch>();
    qDBusRegisterMetaType<RemoteMatches>();
    qDBusRegisterMetaType<RemoteAction>();
    qDBusRegisterMetaType<RemoteActions>();
    qDBusRegisterMetaType<RemoteImage>();
}

bool AbstractKRunnerPlugin::onLoad()
{
    return QDBusConnection::sessionBus().registerObject(m_objectPath, this);
}

bool AbstractKRunnerPlugin::onUnload()
{
    QDBusConnection::sessionBus().unregisterObject(m_objectPath);
    return true;
}

RemoteImage AbstractKRunnerPlugin::serializeImage(const QImage &image)
{
    QImage convertedImage = image.convertToFormat(QImage::Format_RGBA8888);
    RemoteImage remoteImage{
        convertedImage.width(),
        convertedImage.height(),
        convertedImage.bytesPerLine(),
        true, // hasAlpha
        8, // bitsPerSample
        4, // channels
        QByteArray(reinterpret_cast<const char *>(convertedImage.constBits()),
                   convertedImage.sizeInBytes())
    };
    return remoteImage;
}
