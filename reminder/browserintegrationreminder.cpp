/*
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2018 Eike Hein <hein@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "browserintegrationreminder.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QTimer>
#include <QDBusConnectionInterface>
#include <QMenu>
#include <QAction>

#include <KActivities/ResourceInstance>
#include <KConfig>
#include <KConfigGroup>
#include <KIO/ApplicationLauncherJob>
#include <KLocalizedString>
#include <KNotificationJobUiDelegate>
#include <kpluginfactory.h>
#include <KService>
#include <KSharedConfig>
#include <KStatusNotifierItem>

K_PLUGIN_FACTORY_WITH_JSON(BrowserIntegrationReminderFactory,
                           "browserintegrationreminder.json",
                           registerPlugin<BrowserIntegrationReminder>();)

static const QString s_dbusServiceName = QStringLiteral("org.kde.plasma.browser_integration");

#define MAX_SHOW_COUNT 3

BrowserIntegrationReminder::BrowserIntegrationReminder(QObject *parent, const QList<QVariant>&)
      : KDEDModule(parent)
{
    m_debug = qEnvironmentVariableIsSet("PLASMA_BROWSE_REMIND_FORCE");
    auto config = KSharedConfig::openConfig()->group("PlasmaBrowserIntegration");
    m_shownCount = config.readEntry("shownCount", 0);

    if (m_shownCount > MAX_SHOW_COUNT && !m_debug) {
        disableAutoload(); //safer than it looks it won't be processed till we hit the event loop
        return;
    }

    QUrl firefox(QStringLiteral("https://addons.mozilla.org/en-US/firefox/addon/plasma-integration/"));
    m_browsers[QStringLiteral("firefox.desktop")] = firefox;
    m_browsers[QStringLiteral("nightly.desktop")] = firefox;

    QUrl chrome(QStringLiteral("https://chrome.google.com/webstore/detail/plasma-integration/cimiefiiaegbelhefglklhhakcgmhkai"));
    m_browsers[QStringLiteral("google-chrome.desktop")] = chrome;
    m_browsers[QStringLiteral("google-chrome-beta.desktop")] = chrome;
    m_browsers[QStringLiteral("google-chrome-unstable.desktop")] = chrome;
    m_browsers[QStringLiteral("chromium-browser.desktop")] = chrome;
    m_browsers[QStringLiteral("vivaldi-stable.desktop")] = chrome;
    m_browsers[QStringLiteral("brave-browser.desktop")] = chrome;

    QUrl edge(QStringLiteral("https://microsoftedge.microsoft.com/addons/detail/plasma-integration/dnnckbejblnejeabhcmhklcaljjpdjeh"));
    m_browsers[QStringLiteral("microsoft-edge.desktop")] = edge;
    m_browsers[QStringLiteral("microsoft-edge-beta.desktop")] = edge;
    m_browsers[QStringLiteral("microsoft-edge-dev.desktop")] = edge;

    setModuleName(QStringLiteral("BrowserIntegrationReminder"));
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.connect(QStringLiteral("org.kde.ActivityManager"),
                QStringLiteral("/ActivityManager/Resources/Scoring"),
                QStringLiteral("org.kde.ActivityManager.ResourcesScoring"),
                QStringLiteral("ResourceScoreUpdated"),
                this,
                SLOT(onResourceScoresChanged(QString,QString,QString,double,unsigned int,unsigned int)));
}

BrowserIntegrationReminder::~BrowserIntegrationReminder()
{
}

void BrowserIntegrationReminder::onResourceScoresChanged(const QString &activity, const QString &client, const QString &resource, double score, unsigned int lastUpdate, unsigned int firstUpdate)
{
    Q_UNUSED(activity);
    Q_UNUSED(score);
    Q_UNUSED(lastUpdate);
    Q_UNUSED(firstUpdate);
    Q_UNUSED(client)

    if (!resource.startsWith(QStringLiteral("applications:"))) {
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

void BrowserIntegrationReminder::onBrowserStarted(const QString &browser)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (m_sni) {
        return;
    }

    if (!KService::serviceByStorageId(browser)) {
        return;
    }

    if (!m_watcher) {
        m_watcher = new QDBusServiceWatcher(s_dbusServiceName, bus, QDBusServiceWatcher::WatchForRegistration, this);
        connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this](const QString &service) {
            Q_UNUSED(service);
            if (m_sni) {
                m_sni->deleteLater();
                disableAutoload();
            }
        });
    }

    if (!m_debug && bus.interface()->isServiceRegistered(s_dbusServiceName)) {
        //the user has the extension installed, we don't need to keep checking
        //env var exists for easier testing
        disableAutoload();
        return;
    }

    m_sni = new KStatusNotifierItem(this);
    m_shownCount++;
    auto config = KSharedConfig::openConfig()->group("PlasmaBrowserIntegration");
    config.writeEntry("shownCount", m_shownCount);

    m_sni->setTitle(i18n("Get Plasma Browser Integration"));
    m_sni->setIconByName(QStringLiteral("plasma-browser-integration"));
    m_sni->setStandardActionsEnabled(false);
    m_sni->setStatus(KStatusNotifierItem::Active);

    connect(m_sni, &KStatusNotifierItem::activateRequested, this, [this, browser]() {
        const KService::Ptr service = KService::serviceByStorageId(browser);

        if (!service) {
            unload();
            return;
        }

        KIO::ApplicationLauncherJob *job = new KIO::ApplicationLauncherJob(service);
        job->setUrls({m_browsers[browser]});
        job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled));
        job->start();

        KActivities::ResourceInstance::notifyAccessed(QUrl(QStringLiteral("applications:") + browser),
            QStringLiteral("org.kde.plasma.browserintegrationreminder"));
        //remove for this session.
        //If the user installed it successfully we won't show anything next session
        //If they didn't they'll get the link next login
        unload();
    });

    auto *menu = new QMenu;
    auto *action = new QAction(QIcon::fromTheme(QStringLiteral("view-hidden")), i18n("Do not show again"));
    menu->addAction(action);
    connect(action, &QAction::triggered, this, [this]() {
        auto config = KSharedConfig::openConfig()->group("PlasmaBrowserIntegration");
        config.writeEntry("shownCount", 100);
        disableAutoload();
    });

    m_sni->setContextMenu(menu);
}

void BrowserIntegrationReminder::unload()
{
    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kded5"),
                                                      QStringLiteral("/kded"),
                                                      QStringLiteral("org.kde.kded5"),
                                                      QStringLiteral("unloadModule"));
    msg.setArguments({QVariant(QStringLiteral("browserintegrationreminder"))});
    dbus.call(msg, QDBus::NoBlock);
}

void BrowserIntegrationReminder::disableAutoload()
{
    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kded5"),
                                                      QStringLiteral("/kded"),
                                                      QStringLiteral("org.kde.kded5"),
                                                      QStringLiteral("setModuleAutoloading"));
    msg.setArguments({QVariant(QStringLiteral("browserintegrationreminder")), QVariant(false)});
    dbus.call(msg, QDBus::NoBlock);
    unload();
}

#include "browserintegrationreminder.moc"
