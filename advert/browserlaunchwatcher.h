/***************************************************************************
 *   Copyright 2009 by Marco Martin <notmart@gmail.com>                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#pragma once

#include <kdedmodule.h>

#include <QObject>
#include <QStringList>
#include <QSet>
#include <QUrl>
#include <QHash>
#include <QPointer>

class KStatusNotifierItem;
class QDBusServiceWatcher;

/**
 * This plugin's purpose is to provide the user a link to the PBI extension.
 * It should run if the user has installed the host, but doesn't have the browser side.
 *
 * It creates an SNI for the first N times the user opens a browser with this setup.
 * Then we give up
 */

class BrowserLaunchWatcher : public KDEDModule
{
    Q_OBJECT

public:
    BrowserLaunchWatcher(QObject *parent, const QList<QVariant>&);
    ~BrowserLaunchWatcher() override;

private Q_SLOTS:
    void onResourceScoresChanged(const QString &activity,
                            const QString &client,
                            const QString &resource,
                            double score,
                            unsigned int lastUpdate,
                            unsigned int firstUpdate);

    void onBrowserStarted(const QString &browserName);

    void unload();
    void disableAutoload();

private:
    struct BrowserInfo
    {
        QString icon;
        QUrl url;
    };
    QHash<QString, BrowserInfo> m_browsers;
    QPointer<KStatusNotifierItem> m_sni;
    bool m_debug;
    int m_shownCount;
};
