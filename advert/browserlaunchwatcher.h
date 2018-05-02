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

class QDBusServiceWatcher;

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
private:
    struct BrowserInfo
    {
        QString icon;
        QUrl extensionUrl;
    }
    QHash<QString, BrowserInfo> m_browsers;
};
