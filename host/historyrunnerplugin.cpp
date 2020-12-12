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

#include "historyrunnerplugin.h"

#include "connection.h"
#include "settings.h"

#include <QDBusConnection>
#include <QIcon>
#include <QPixmap>
#include <QSet>
#include <QUrl>
#include <QVariant>

#include <KLocalizedString>

#include <algorithm>

#include "krunner1adaptor.h"

static const auto s_idSeparator = QLatin1String("@@@");

static const auto s_errorNoPermission = QLatin1String("NO_PERMISSION");
static const auto s_idRequestPermission = QLatin1String("REQUEST_PERMISSION");

HistoryRunnerPlugin::HistoryRunnerPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("historyrunner"), 1, parent)
{
    new Krunner1Adaptor(this);
    qDBusRegisterMetaType<RemoteMatch>();
    qDBusRegisterMetaType<RemoteMatches>();
    qDBusRegisterMetaType<RemoteAction>();
    qDBusRegisterMetaType<RemoteActions>();
    qDBusRegisterMetaType<RemoteImage>();
}

bool HistoryRunnerPlugin::onLoad()
{
    return QDBusConnection::sessionBus().registerObject(QStringLiteral("/HistoryRunner"), this);
}

bool HistoryRunnerPlugin::onUnload()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/HistoryRunner"));
    return true;
}

RemoteActions HistoryRunnerPlugin::Actions()
{
    RemoteActions actions;
    return actions;
}

RemoteMatches HistoryRunnerPlugin::Match(const QString &searchTerm)
{
    RemoteMatches matches;
    // Shouldn't happen but someone could cal this directly...
    if (searchTerm.length() < 3) {
        // TODO send error reply
        return matches;
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

    return matches;
}

void HistoryRunnerPlugin::Run(const QString &id, const QString &actionId)
{
    if (!actionId.isEmpty()) {
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
            match.iconName = Settings::self().environmentDescription().iconName;
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
                match.iconName = Settings::self().environmentDescription().iconName;

                QUrl urlWithoutPassword = url;
                urlWithoutPassword.setPassword({});
                match.properties.insert(QStringLiteral("urls"), QUrl::toStringList(QList<QUrl>{urlWithoutPassword}));

                if (favIconUrl.startsWith(QLatin1String("data:"))) {
                    const int b64start = favIconUrl.indexOf(QLatin1Char(','));
                    if (b64start != -1) {
                        QByteArray b64 = favIconUrl.rightRef(favIconUrl.count() - b64start - 1).toLatin1();
                        QByteArray data = QByteArray::fromBase64(b64);
                        QImage image;
                        if (image.loadFromData(data)) {
                            // FIXME agree on a pixel format
                            QImage convertedImage = image.convertToFormat(QImage::Format_RGBA8888);
                            RemoteImage remoteImage{
                                convertedImage.width(),
                                convertedImage.height(),
                                convertedImage.bytesPerLine(),
                                true, // hasAlpha
                                8, // bitsPerSample
                                4, // channels
                                QByteArray(reinterpret_cast<const char *>(convertedImage.constBits()), convertedImage.sizeInBytes())
                            };
                            match.properties.insert(QStringLiteral("icon-data"), QVariant::fromValue(remoteImage));
                        } else {
                            qWarning() << "Failed to load favicon image for" << match.id << match.text;
                        }
                    }
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

