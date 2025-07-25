/*
    SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "abstractkrunnerplugin.h"

#include <QDBusConnection>
#include <QGuiApplication>
#include <QImage>

#include "krunner1adaptor.h"

AbstractKRunnerPlugin::AbstractKRunnerPlugin(const QString &objectPath, const QString &subsystemId, int protocolVersion, QObject *parent)
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

    const QByteArray data = dataFromDataUrl(dataUrl);
    if (data.isEmpty()) {
        return image;
    }

    if (!image.loadFromData(data)) {
        qWarning() << "Failed to load favicon image from" << dataUrl.left(30);
    }

    // Limit image size to reduce DBus message size.
    const int maxIconSize = 64 * qGuiApp->devicePixelRatio();
    if (image.width() > maxIconSize || image.height() > maxIconSize) {
        image = image.scaled(maxIconSize, maxIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}

RemoteImage AbstractKRunnerPlugin::serializeImage(const QImage &image)
{
    QImage convertedImage = image.convertToFormat(QImage::Format_RGBA8888);
    RemoteImage remoteImage{
        convertedImage.width(),
        convertedImage.height(),
        static_cast<int>(convertedImage.bytesPerLine()),
        true, // hasAlpha
        8, // bitsPerSample
        4, // channels
        QByteArray(reinterpret_cast<const char *>(convertedImage.constBits()), convertedImage.sizeInBytes()),
    };
    return remoteImage;
}

#include "moc_abstractkrunnerplugin.cpp"
