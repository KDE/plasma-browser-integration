/*
    SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "abstractbrowserplugin.h"

#include <QDBusContext>

// FIXME adaptor needs this, can we tell qt5_add_dbus_adaptor to add it to the generated file?
#include "dbusutils_p.h"

class AbstractKRunnerPlugin : public AbstractBrowserPlugin, protected QDBusContext
{
    Q_OBJECT

protected:
    AbstractKRunnerPlugin(const QString &objectPath,
                          const QString &subsystemId,
                          int protocolVersion,
                          QObject *parent);

    static QImage imageFromDataUrl(const QString &dataUrl);
    static RemoteImage serializeImage(const QImage &image);

public:
    bool onLoad() override;
    bool onUnload() override;

    // DBus API
    virtual RemoteActions Actions() = 0;
    virtual RemoteMatches Match(const QString &searchTerm) = 0;
    virtual void Run(const QString &id, const QString &actionId) = 0;

private:
    QString m_objectPath;

};
