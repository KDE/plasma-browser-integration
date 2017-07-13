/*
    Copyright (C) 2017 by Kai Uwe Broulik <kde@privat.broulik.de>
    Copyright (C) 2017 by David Edmundson <davidedmundson@kde.org>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

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
    AbstractBrowserPlugin(QStringLiteral("tabsrunner"), 1, parent)
{
    new TabsRunnerAdaptor(this);
}

bool TabsRunnerPlugin::onLoad()
{
    return QDBusConnection::sessionBus().registerObject(QStringLiteral("/TabsRunner"), this);
}

bool TabsRunnerPlugin::onUnload()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/TabsRunner"));
    return true;
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
        return {};
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

void TabsRunnerPlugin::SetMuted(int tabId, bool muted)
{
    sendData(QStringLiteral("setMuted"), {
        {QStringLiteral("tabId"), tabId},
        {QStringLiteral("muted"), muted}
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
                m_tabsReplyMessage.createReply(QList<QVariant>{tabsReply})
            );
            m_tabsReplyMessage = QDBusMessage();
        }
    }
}

