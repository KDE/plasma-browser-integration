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
#include <QDBusMessage>
#include <QDesktopServices>
#include <QTimer>
#include <QDBusConnectionInterface>
#include <QMenu>
#include <QAction>

#include <kpluginfactory.h>
#include <KStatusNotifierItem>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfig>
#include <KConfigGroup>

#include "browserlaunchwatcher.h"

K_PLUGIN_FACTORY_WITH_JSON(BrowserLaunchWatcherFactory,
                           "browserlaunchwatcher.json",
                           registerPlugin<BrowserLaunchWatcher>();)

#define MAX_SHOW_COUNT 3

BrowserLaunchWatcher::BrowserLaunchWatcher(QObject *parent, const QList<QVariant>&)
      : KDEDModule(parent)
{
    m_debug = qEnvironmentVariableIsSet("PLASMA_BROWSE_REMIND_FORCE");
    auto config = KSharedConfig::openConfig()->group("PlasmaBrowserIntegration");
    m_shownCount = config.readEntry("shownCount", 0);

    if (m_shownCount > MAX_SHOW_COUNT && !m_debug) {
        disableAutoload(); //safer than it looks it won't be processed till we hit the event loop
        return;
    }

    m_browsers["firefox.desktop"] = {.icon="firefox", .url=QUrl("https://addons.mozilla.org/en-US/firefox/addon/plasma-integration/")};
    BrowserInfo chrome  = {.icon="google-chrome", .url=QUrl("https://chrome.google.com/webstore/detail/plasma-integration/cimiefiiaegbelhefglklhhakcgmhkai")};
    m_browsers["google-chrome.desktop"] = chrome;
    m_browsers["google-chrome-unstable.desktop"] = chrome;
    m_browsers["chromium-browser.desktop"] = {.icon="chromium-browser", .url=QUrl("https://chrome.google.com/webstore/detail/plasma-integration/cimiefiiaegbelhefglklhhakcgmhkai")};

    setModuleName(QStringLiteral("BrowserLaunchWatcher"));
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.connect(QStringLiteral("org.kde.ActivityManager"),
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
    Q_UNUSED(activity);
    Q_UNUSED(score);
    Q_UNUSED(lastUpdate);
    Q_UNUSED(firstUpdate);
    Q_UNUSED(client)

    if (!resource.startsWith("applications:")) {
        return;
    }
    const QString desktopFile = resource.mid(strlen("applications:"));
    if (!m_browsers.contains(desktopFile)) {
        return;
    }
    //wait a few seconds and then query if the extension is active
    QTimer::singleShot(10 * 1000, this, [this, desktopFile]() {
        onBrowserStarted(desktopFile);
    });
}

void BrowserLaunchWatcher::onBrowserStarted(const QString &browser)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (m_sni) {
        return;
    }

    if (bus.interface()->isServiceRegistered(QStringLiteral("org.kde.plasma.browser_integration")) && !m_debug) {
        //the user has the extension installed, we don't need to keep checking
        //env var exists for easier testing
        disableAutoload();
        return;
    }

    auto info = m_browsers[browser];

    m_sni = new KStatusNotifierItem(this);
    m_shownCount++;
    auto config = KSharedConfig::openConfig()->group("PlasmaBrowserIntegration");
    config.writeEntry("shownCount", m_shownCount);

    m_sni->setTitle(i18n("Get Plasma Browser Integration"));
    m_sni->setIconByName(info.icon);
    m_sni->setStandardActionsEnabled(false);
    m_sni->setStatus(KStatusNotifierItem::Active);

    connect(m_sni, &KStatusNotifierItem::activateRequested, this, [this, info]() {
        QDesktopServices::openUrl(info.url);
        //remove for this session.
        //If the user installed it successfully we won't show anything next session
        //If they didn't they'll get the link next login
        unload();
    });

    QMenu *menu = new QMenu;
    auto action = new QAction(i18n("Do not show again"));
    menu->addAction(action);
    connect(action, &QAction::triggered, this, [this]() {
        auto config = KSharedConfig::openConfig()->group("PlasmaBrowserIntegration");
        config.writeEntry("shownCount", 100);
        disableAutoload();
    });

    m_sni->setContextMenu(menu);
}

void BrowserLaunchWatcher::unload()
{
    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kded5"),
                                                      QStringLiteral("/kded"),
                                                      QStringLiteral("org.kde.kded5"),
                                                      QStringLiteral("unloadModule"));
    msg.setArguments({QVariant(QStringLiteral("browserlaunchwatcher"))});
    dbus.call(msg, QDBus::NoBlock);
}

void BrowserLaunchWatcher::disableAutoload()
{
    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kded5"),
                                                      QStringLiteral("/kded"),
                                                      QStringLiteral("org.kde.kded5"),
                                                      QStringLiteral("setModuleAutoloading"));
    msg.setArguments({QVariant(QStringLiteral("browserlaunchwatcher")), QVariant(false)});
    dbus.call(msg, QDBus::NoBlock);
    unload();
}

#include "browserlaunchwatcher.moc"
