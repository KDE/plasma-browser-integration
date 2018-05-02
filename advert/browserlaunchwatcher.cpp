/***************************************************************************
 *   Copyright 2018 by David Edmundson <davidedmundson@kde.org>            *
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

#include "browserlaunchwatcher.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QDesktopServices>

#include <kpluginfactory.h>
#include <KStatusNotifierItem>
#include <KLocalizedString>

#include "browserlaunchwatcher.h"

K_PLUGIN_FACTORY_WITH_JSON(BrowserLaunchWatcherFactory,
                           "browserlaunchwatcher.json",
                           registerPlugin<BrowserLaunchWatcher>();)



BrowserLaunchWatcher::BrowserLaunchWatcher(QObject *parent, const QList<QVariant>&)
      : KDEDModule(parent)
{

    //if config >= 0 && < 3 ; return


    m_browsers["firefox.desktop"] = BrowserInfo({"firefox", "https://addons.mozilla.org/en-US/firefox/addon/plasma-integration/"});
    BrowserInfo chrome({"google-chrome", "https://chrome.google.com/webstore/detail/plasma-integration/cimiefiiaegbelhefglklhhakcgmhkai"});
    m_browsers["google-chrome.desktop"] = chrome;
    m_browsers["google-chrome-unstable.desktop"] = chrome;
    m_browsers["chromium-browser.desktop"] = ({"chromium-browser", "https://chrome.google.com/webstore/detail/plasma-integration/cimiefiiaegbelhefglklhhakcgmhkai"});

    setModuleName(QStringLiteral("BrowserLaunchWatcher"));
    QDBusConnection dbus = QDBusConnection::sessionBus();
    qDebug() << dbus.connect(QStringLiteral("org.kde.ActivityManager"),
                QStringLiteral("/ActivityManager/Resources/Scoring"),
                QStringLiteral("org.kde.ActivityManager.ResourcesScoring"),
                QStringLiteral("ResourceScoreUpdated"),
                this,
                SLOT(onResourceScoresChanged(QString,QString,QString,double,unsigned int,unsigned int)));
}

BrowserLaunchWatcher::~BrowserLaunchWatcher()
{
}

void BrowserLaunchWatcher::onResourceScoresChanged(const QString &activity, const QString &client, const QString &resource, double score, unsigned int lastUpdate, unsigned int firstUpdate)
{

    if (!resource.startsWith("applications:")) {
        return;
    }
    const QString desktopFile = resource.mid(strlen("applications:"));
    qDebug() << desktopFile;
    if (!m_browsers.contains(desktopFile)) {
        return;
    }
    auto info = m_browsers[desktopFile];
    auto sni = new KStatusNotifierItem(this);
    sni->setTitle(i18n("Get Plasma Browser Integration"));
    sni->setIconByName("plasma-browser-integration");
    sni->setStandardActionsEnabled(false);

    //if m_sni return;
    //wait 10 seconds
    //check for host
    //increment config
    //make sni member variable
    //show message
    //counter++

    //void disable()
    //set modue autoloading false
    //

    connect(sni, &KStatusNotifierItem::activateRequest, this, [this, info]() {
        QDesktopServices::openUrl(info.extensionUrl);
    });
}

#include "browserlaunchwatcher.moc"
