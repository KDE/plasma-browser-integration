/*
 *   Copyright (C) 2017 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "tabsrunner.h"

#include <QMimeData>

#include <QUrl>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>

#include <KLocalizedString>

TabsRunner::TabsRunner(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    Q_UNUSED(args)

    setObjectName(QStringLiteral("BrowserTabs"));
    setPriority(AbstractRunner::HighestPriority);

    addSyntax(Plasma::RunnerSyntax(QStringLiteral(":q:"), i18n("Finds browser tabs whose title match :q:")));
}

TabsRunner::~TabsRunner() = default;

void TabsRunner::match(Plasma::RunnerContext &context)
{
    const QString &term = context.query();
    if (term.length() < 3 && !context.singleRunnerQueryMode()) {
        return;
    }

    QDBusMessage message =
        QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasma.browser_integration"),
                                       QStringLiteral("/TabsRunner"),
                                       QStringLiteral("org.kde.plasma.browser_integration.TabsRunner"),
                                       QStringLiteral("GetTabs")
        );

    QDBusMessage reply = QDBusConnection::sessionBus().call(message);

    if (reply.type() == QDBusMessage::ReplyMessage) {

        QList<Plasma::QueryMatch> matches;

        // TODO why does it unwrap this? didn't we want a a(a{sv}) instead of a{sv}a{sv}a{sv}..? :/
        for (const auto &arg : reply.arguments()) {
            QVariantHash tab;
            // urgh?
            arg.value<QDBusArgument>() >> tab;

            // add browser name or window name or so to it maybe?
            const QString &text = tab.value(QStringLiteral("title")).toString();
            if (text.isEmpty()) { // shouldn't happen?
                continue;
            }

            // will be used to raise the tab eventually
            int tabId = tab.value(QStringLiteral("id")).toInt();
            if (!tabId) {
                continue;
            }

            const QUrl url(tab.value(QStringLiteral("url")).toString());
            if (!url.isValid()) {
                continue;
            }

            Plasma::QueryMatch match(this);
            match.setText(text);
            match.setData(tabId);
            match.setSubtext(url.toDisplayString());

            qreal relevance = 0;

            // someone was really busy here, typing the *exact* title or url :D
            if (text.compare(term, Qt::CaseInsensitive) == 0 || url.toString().compare(term, Qt::CaseInsensitive) == 0) {
                match.setType(Plasma::QueryMatch::ExactMatch);
                relevance = 1;
            } else {
                match.setType(Plasma::QueryMatch::PossibleMatch);

                if (text.contains(term, Qt::CaseInsensitive)) {
                    relevance = 0.9;
                    if (text.startsWith(term, Qt::CaseInsensitive)) {
                        relevance += 0.1;
                    }
                } else if (url.host().contains(term, Qt::CaseInsensitive)) {
                    relevance = 0.7;
                    if (url.host().startsWith(term, Qt::CaseInsensitive)) {
                        relevance += 0.1;
                    }
                } else if (url.path().contains(term, Qt::CaseInsensitive)) {
                    relevance = 0.5;
                    if (url.path().startsWith(term, Qt::CaseInsensitive)) {
                        relevance += 0.1;
                    }
                }
            }

            if (!relevance) {
                continue;
            }

            match.setRelevance(relevance);

            match.setIconName(QStringLiteral("google-chrome")); // TODO favicon or at least correct browser icon

            matches << match;
        }

        context.addMatches(matches);
    }
}

void TabsRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context);

    QDBusMessage message =
        QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasma.browser_integration"),
                                       QStringLiteral("/TabsRunner"),
                                       QStringLiteral("org.kde.plasma.browser_integration.TabsRunner"),
                                       QStringLiteral("Activate")
        );
    message.setArguments({match.data().toInt()});

    QDBusConnection::sessionBus().call(message); // asyncCall?
}

QMimeData *TabsRunner::mimeDataForMatch(const Plasma::QueryMatch &match)
{
    Q_UNUSED(match);
    // TODO return tab url or maybe for firefox a magic "dragging tab off a window" mime?
    return nullptr;
}

K_EXPORT_PLASMA_RUNNER(browsertabs, TabsRunner)

#include "tabsrunner.moc"
