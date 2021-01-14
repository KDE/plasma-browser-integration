/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "tabsrunnerplugin.h"

#include "connection.h"

#include <QDBusConnection>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>

#include <KApplicationTrader>
#include <KLocalizedString>

#include "settings.h"

static const auto s_actionIdMute = QLatin1String("MUTE");
static const auto s_actionIdUnmute = QLatin1String("UNMUTE");

TabsRunnerPlugin::TabsRunnerPlugin(QObject* parent) :
    AbstractKRunnerPlugin(QStringLiteral("/TabsRunner"),
                          QStringLiteral("tabsrunner"),
                          1,
                          parent)
{

}

RemoteActions TabsRunnerPlugin::Actions()
{
    RemoteAction muteAction{
        s_actionIdMute,
        i18n("Mute Tab"),
        QStringLiteral("audio-volume-muted")
    };
    RemoteAction unmuteAction{
        s_actionIdUnmute,
        i18n("Unmute Tab"),
        QStringLiteral("audio-volume-high")
    };

    return {muteAction, unmuteAction};
}

RemoteMatches TabsRunnerPlugin::Match(const QString &searchTerm)
{
    if (searchTerm.length() < 3) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Search term too short"));
        return {};
    }

    setDelayedReply(true);

    const bool runQuery = m_requests.isEmpty();

    m_requests.insert(searchTerm, message());

    if (runQuery) {
        sendData(QStringLiteral("getTabs"));
    }

    return {};
}

void TabsRunnerPlugin::Run(const QString &id, const QString &actionId)
{
    bool ok = false;
    const int tabId = id.toInt(&ok);
    if (!ok || tabId < 0) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid tab ID"));
        return;
    }

    if (actionId.isEmpty()) {
        sendData(QStringLiteral("activate"), {
            {QStringLiteral("tabId"), tabId}
        });
        return;
    }

    if (actionId == s_actionIdMute || actionId == s_actionIdUnmute) {
        sendData(QStringLiteral("setMuted"), {
            {QStringLiteral("tabId"), tabId},
            {QStringLiteral("muted"), actionId == s_actionIdMute}
        });
        return;
    }

    sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Unknown action ID"));
}

void TabsRunnerPlugin::handleData(const QString& event, const QJsonObject& json)
{
    if (event == QLatin1String("gotTabs")) {

        const QJsonArray &tabs = json.value(QStringLiteral("tabs")).toArray();

        for (auto it = m_requests.constBegin(), end = m_requests.constEnd(); it != end; ++it) {
            const QString query = it.key();
            const QDBusMessage request = it.value();

            RemoteMatches matches;

            for (auto jt = tabs.constBegin(), jend = tabs.constEnd(); jt != jend; ++jt) {
                const QJsonObject tab = jt->toObject();

                RemoteMatch match;

                const int tabId = tab.value(QStringLiteral("id")).toInt();
                const QString text = tab.value(QStringLiteral("title")).toString();
                const QUrl url(tab.value(QStringLiteral("url")).toString());

                QStringList actions;

                qreal relevance = 0;
                // someone was really busy here, typing the *exact* title or url :D
                if (text.compare(query, Qt::CaseInsensitive) == 0
                        || url.toString().compare(query, Qt::CaseInsensitive) == 0) {
                    match.type = Plasma::QueryMatch::ExactMatch;
                    relevance = 1;
                } else {
                    match.type = Plasma::QueryMatch::PossibleMatch;

                    if (KApplicationTrader::isSubsequence(query, text, Qt::CaseInsensitive)) {
                        relevance = 0.85;
                        if (text.contains(query, Qt::CaseInsensitive)) {
                            relevance += 0.05;
                            if (text.startsWith(query, Qt::CaseInsensitive)) {
                                relevance += 0.05;
                            }
                        }
                    } else if (url.host().contains(query, Qt::CaseInsensitive)) {
                        relevance = 0.7;
                        if (url.host().startsWith(query, Qt::CaseInsensitive)) {
                            relevance += 0.1;
                        }
                    } else if (url.path().contains(query, Qt::CaseInsensitive)) {
                        relevance = 0.5;
                        if (url.path().startsWith(query, Qt::CaseInsensitive)) {
                            relevance += 0.1;
                        }
                    }
                }

                if (!relevance) {
                    continue;
                }

                match.id = QString::number(tabId);
                match.text = text;
                match.properties.insert(QStringLiteral("subtext"), url.toDisplayString());
                match.relevance = relevance;

                const bool audible = tab.value(QStringLiteral("audible")).toBool();

                const QJsonObject mutedInfo = tab.value(QStringLiteral("mutedInfo")).toObject();
                const bool muted = mutedInfo.value(QStringLiteral("muted")).toBool();

                if (audible) {
                    if (muted) {
                        match.iconName = QStringLiteral("audio-volume-muted");
                        actions.append(s_actionIdUnmute);
                    } else {
                        match.iconName = QStringLiteral("audio-volume-high");
                        actions.append(s_actionIdMute);
                    }
                } else {
                    match.iconName = qApp->windowIcon().name();

                    const QImage favIcon = imageFromDataUrl(tab.value(QStringLiteral("favIconData")).toString());
                    if (!favIcon.isNull()) {
                        const RemoteImage remoteImage = serializeImage(favIcon);
                        match.properties.insert(QStringLiteral("icon-data"), QVariant::fromValue(remoteImage));
                    }
                }

                // Has to always be present so it knows we handle actions ourself
                match.properties.insert(QStringLiteral("actions"), actions);

                matches.append(match);
            }

            QDBusConnection::sessionBus().send(
                request.createReply(QVariant::fromValue(matches))
            );
        }

        m_requests.clear();
    }
}
