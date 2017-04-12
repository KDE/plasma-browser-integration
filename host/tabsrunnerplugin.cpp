#include "tabsrunnerplugin.h"

#include "connection.h"

#include <QDBusConnection>
#include <QDebug>

#include <QList>
#include <QHash>
#include <QVariant>
#include <QVariantHash>

#include "tabsrunneradaptor.h"

TabsRunnerPlugin::TabsRunnerPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("tabsrunner"), parent)
{
    new TabsRunnerAdaptor(this);

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/TabsRunner"), this);
}

// FIXME We really should enforce some kind of security policy, so only e.g. plasmashell and krunner
// may access your tabs
QList<QVariantHash> TabsRunnerPlugin::GetTabs()
{
    // already a get tabs request pending, abort it and then start anew
    // TODO would be lovely to marshall that stuff somehow, ie. every GetTabs call gets
    // its own reply instead of just aborting and serving the one who came last
    // however, the TabsRunner blocks waiting for a reply, so in practise this isn't as urgent
    if (m_tabsReplyMessage.type() != QDBusMessage::InvalidMessage) {
        QDBusConnection::sessionBus().send(
            m_tabsReplyMessage.createErrorReply(
                QStringLiteral("org.kde.plasma.browser_integration.TabsRunner.Error.Cancelled"),
                QStringLiteral("GetTabs got cancelled because a another request came in")
            )
        );
    }

    m_tabsReplyMessage = message();
    setDelayedReply(true);

    sendData(QStringLiteral("getTabs"));

    return {};
}

void TabsRunnerPlugin::Activate(int tabId)
{
    sendData(QStringLiteral("activate"), {
        {QStringLiteral("tabId"), tabId}
    });
}

void TabsRunnerPlugin::handleData(const QString& event, const QJsonObject& json)
{
    if (event == QLatin1String("gotTabs")) {
        if (m_tabsReplyMessage.type() != QDBusMessage::InvalidMessage) {

            const QJsonArray &tabs = json.value(QStringLiteral("tabs")).toArray();

            QList<QVariant> tabsReply;
            tabsReply.reserve(tabs.count());

            for (auto it = tabs.constBegin(), end = tabs.constEnd(); it != end; ++it) {
                const QJsonObject &tab = it->toObject();

                tabsReply.append(tab.toVariantHash());
            }

            QList<QVariant> reply;
            reply.append(QVariant(tabsReply));

            QDBusConnection::sessionBus().send(
                // TODO why does it unwrap this? didn't we want a a(a{sv}) instead of a{sv}a{sv}a{sv}..? :/
                m_tabsReplyMessage.createReply({tabsReply})
            );
        }
    }
}

