/*
    SPDX-FileCopyrightText: 2009 Marco Martin <notmart@gmail.com>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2018 Eike Hein <hein@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

class BrowserIntegrationReminder : public KDEDModule
{
    Q_OBJECT

public:
    BrowserIntegrationReminder(QObject *parent, const QList<QVariant>&);
    ~BrowserIntegrationReminder() override;

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
    QHash<QString, QUrl> m_browsers;
    QPointer<KStatusNotifierItem> m_sni;
    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_debug;
    int m_shownCount;
};
