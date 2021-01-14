/*
    SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "historyrunnerplugin.h"

#include "connection.h"
#include "settings.h"

#include <QDBusConnection>
#include <QGuiApplication>
#include <QJsonArray>
#include <QIcon>
#include <QImage>
#include <QSet>
#include <QUrl>
#include <QVariant>

#include <KLocalizedString>

#include <algorithm>

static const auto s_idSeparator = QLatin1String("@@@");

static const auto s_errorNoPermission = QLatin1String("NO_PERMISSION");
static const auto s_idRequestPermission = QLatin1String("REQUEST_PERMISSION");

HistoryRunnerPlugin::HistoryRunnerPlugin(QObject *parent)
    : AbstractKRunnerPlugin(QStringLiteral("/HistoryRunner"),
                            QStringLiteral("historyrunner"),
                            1,
                            parent)
{

}

RemoteActions HistoryRunnerPlugin::Actions()
{
    return {};
}

RemoteMatches HistoryRunnerPlugin::Match(const QString &searchTerm)
{
    if (searchTerm.length() < 3) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Search term too short"));
        return {};
    }

    setDelayedReply(true);

    const bool runQuery = !m_requests.contains(searchTerm);

    // It's a multi-hash, so all requests for identical search terms
    // will be replied to at once when the results come in
    m_requests.insert(searchTerm, message());

    if (runQuery) {
        sendData(QStringLiteral("find"), {
            {QStringLiteral("query"), searchTerm}
        });
    }

    return {};
}

void HistoryRunnerPlugin::Run(const QString &id, const QString &actionId)
{
    if (!actionId.isEmpty()) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Unknown action ID"));
        return;
    }

    if (id == s_idRequestPermission) {
        sendData(QStringLiteral("requestPermission"));
        return;
    }

    const int separatorIdx = id.indexOf(s_idSeparator);
    if (separatorIdx <= 0) {
        return;
    }

    const QString historyId = id.left(separatorIdx);
    const QString urlString = id.mid(separatorIdx + s_idSeparator.size());

    // Ideally we'd run the "id" but there's no API to query a history item by id.
    // To be future proof, we'll send both, just in case there's an "id" API at some point
    sendData(QStringLiteral("run"), {
        // NOTE Chromium uses ints but Firefox returns ID strings, so don't toInt() this!
        {QStringLiteral("id"), historyId},
        {QStringLiteral("url"), urlString}
    });
}

void HistoryRunnerPlugin::handleData(const QString& event, const QJsonObject& json)
{
    if (event == QLatin1String("found")) {
        const QString query = json.value(QStringLiteral("query")).toString();
        const QString error = json.value(QStringLiteral("error")).toString();

        RemoteMatches matches;

        if (error == s_errorNoPermission) {
            RemoteMatch match;
            match.id = s_idRequestPermission;
            match.type = Plasma::QueryMatch::NoMatch;
            match.relevance = 0;
            match.text = i18nc("Dummy search result", "Additional permissions are required");
            match.iconName = qApp->windowIcon().name();
            matches.append(match);
        } else {
            const QJsonArray results = json.value(QStringLiteral("results")).toArray();

            int maxVisitCount = 0;
            int maxTypedCount = 0;

            QSet<QUrl> seenUrls;
            for (auto it = results.begin(), end = results.end(); it != end; ++it) {
                const QJsonObject &result = it->toObject();

                const QString urlString = result.value(QStringLiteral("url")).toString();
                QUrl url(urlString);
                // Skip page anchors but only if they don't look like paths used by old Ajax pages
                const QString urlFragment = url.fragment();
                if (!urlFragment.isEmpty() && !urlFragment.contains(QLatin1Char('/'))) {
                    url.setFragment(QString());
                }

                if (seenUrls.contains(url)) {
                    continue;
                }
                seenUrls.insert(url);

                const QString id = result.value(QStringLiteral("id")).toString();
                const QString text = result.value(QStringLiteral("title")).toString();
                const int visitCount = result.value(QStringLiteral("visitCount")).toInt();
                const int typedCount = result.value(QStringLiteral("typedCount")).toInt();
                const QString favIconUrl = result.value(QStringLiteral("favIconUrl")).toString();

                maxVisitCount = std::max(maxVisitCount, visitCount);
                maxTypedCount = std::max(maxTypedCount, typedCount);

                RemoteMatch match;
                match.id = id + s_idSeparator + urlString;
                if (!text.isEmpty()) {
                    match.text = text;
                    match.properties.insert(QStringLiteral("subtext"), url.toDisplayString());
                } else {
                    match.text = url.toDisplayString();
                }
                match.iconName = qApp->windowIcon().name();

                QUrl urlWithoutPassword = url;
                urlWithoutPassword.setPassword({});
                match.properties.insert(QStringLiteral("urls"), QUrl::toStringList(QList<QUrl>{urlWithoutPassword}));

                const QImage favIcon = imageFromDataUrl(favIconUrl);
                if (!favIcon.isNull()) {
                    const RemoteImage remoteImage = serializeImage(favIcon);
                    match.properties.insert(QStringLiteral("icon-data"), QVariant::fromValue(remoteImage));
                }

                qreal relevance = 0;

                if (text.compare(query, Qt::CaseInsensitive) == 0
                        || urlString.compare(query, Qt::CaseInsensitive) == 0) {
                    match.type = Plasma::QueryMatch::ExactMatch;
                    relevance = 1;
                } else {
                    match.type = Plasma::QueryMatch::PossibleMatch;

                    if (text.contains(query, Qt::CaseInsensitive)) {
                        relevance = 0.7;
                        if (text.startsWith(query, Qt::CaseInsensitive)) {
                            relevance += 0.05;
                        }
                    } else if (url.host().contains(query, Qt::CaseInsensitive)) {
                        relevance = 0.5;
                        if (url.host().startsWith(query, Qt::CaseInsensitive)) {
                            relevance += 0.05;
                        }
                    } else if (url.path().contains(query, Qt::CaseInsensitive)) {
                        relevance = 0.3;
                        if (url.path().startsWith(query, Qt::CaseInsensitive)) {
                            relevance += 0.05;
                        }
                    }
                }

                match.relevance = relevance;

                matches.append(match);
            }

            // Now slightly weigh the results also by visited and typed count
            // The more visits, the higher the relevance boost, but typing counts even higher than visited
            // TODO also take into account lastVisitTime?
            for (int i = 0; i < matches.count(); ++i) {
                RemoteMatch &match = matches[i];

                const QJsonObject result = results.at(i).toObject();
                const int visitCount = result.value(QStringLiteral("visitCount")).toInt();
                const int typedCount = result.value(QStringLiteral("typedCount")).toInt();

                if (maxVisitCount > 0) {
                    const qreal visitBoost = visitCount / static_cast<qreal>(maxVisitCount);
                    match.relevance += visitBoost * 0.05;
                }
                if (maxTypedCount > 0) {
                    const qreal typedBoost = typedCount / static_cast<qreal>(maxTypedCount);
                    match.relevance += typedBoost * 0.1;
                }
            }
        }

        const auto requests = m_requests.values(query);
        m_requests.remove(query); // is there a takeAll?
        for (const QDBusMessage &request : requests) {
            QDBusConnection::sessionBus().send(
                request.createReply(QVariant::fromValue(matches))
            );
        }
    }
}

