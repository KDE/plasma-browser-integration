/*
    SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-3.0-or-later
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

QImage AbstractKRunnerPlugin::imageFromDataUrl(const QString &dataUrl)
{
    QImage image;

    if (!dataUrl.startsWith(QLatin1String("data:"))) {
        return image;
    }

    const int b64start = dataUrl.indexOf(QLatin1Char(','));
    if (b64start == -1) {
        qWarning() << "Invalid data URL format for" << dataUrl.left(30);
        return image;
    }

    const QByteArray b64 = dataUrl.rightRef(dataUrl.count() - b64start - 1).toLatin1();
    const QByteArray data = QByteArray::fromBase64(b64);

    if (!image.loadFromData(data)) {
        qWarning() << "Failed to load favicon image from" << dataUrl.left(30);
    }
    return image;
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
