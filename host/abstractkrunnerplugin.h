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
