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

static const QString s_muteTab = QStringLiteral("mute");
static const QString s_unmuteTab = QStringLiteral("unmute");

TabsRunner::TabsRunner(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    Q_UNUSED(args)

    setObjectName(QStringLiteral("BrowserTabs"));
    setPriority(AbstractRunner::HighestPriority);

    addSyntax(Plasma::RunnerSyntax(QStringLiteral(":q:"), i18n("Finds browser tabs whose title match :q:")));

    // should we actually show the current state instead of what the button will do?
    addAction(s_muteTab, QIcon::fromTheme(QStringLiteral("audio-volume-muted")), i18n("Mute Tab"));
    addAction(s_unmuteTab, QIcon::fromTheme(QStringLiteral("audio-volume-high")), i18n("Unmute Tab"));

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

            const bool incognito = tab.value(QStringLiteral("incognito")).toBool();
            const bool audible = tab.value(QStringLiteral("audible")).toBool();

            QVariantHash mutedInfo;
            tab.value(QStringLiteral("mutedInfo")).value<QDBusArgument>() >> mutedInfo;

            const bool muted = mutedInfo.value(QStringLiteral("muted")).toBool();

            const QVariantHash tabData = {
                {QStringLiteral("tabId"), tabId},
                {QStringLiteral("audible"), audible},
                {QStringLiteral("muted"), muted}
            };

            Plasma::QueryMatch match(this);
            match.setText(text);
            match.setData(tabData);

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

            QString iconName = QStringLiteral("google-chrome"); // TODO favicon or at least correct browser icon

            if (incognito) {
                iconName = QStringLiteral("face-smirk");// TODO QStringLiteral("incognito");
            }

            if (audible) {
                if (muted) {
                    iconName = QStringLiteral("audio-volume-muted");
                } else {
                    iconName = QStringLiteral("audio-volume-high");
                }
            }

            match.setIconName(iconName);

            matches << match;
        }

        context.addMatches(matches);
    }
}

void TabsRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context);

    const int tabId = match.data().toHash().value(QStringLiteral("tabId")).toInt();

    if (match.selectedAction() == action(s_unmuteTab)) {
        QDBusMessage message = createMessage(QStringLiteral("SetMuted"));
        message.setArguments({tabId, false});
        QDBusConnection::sessionBus().call(message); // asyncCall?
        return;
    }

    if (match.selectedAction() == action(s_muteTab)) {
        QDBusMessage message = createMessage(QStringLiteral("SetMuted"));
        message.setArguments({tabId, true});
        QDBusConnection::sessionBus().call(message); // asyncCall?
        return;
    }

    QDBusMessage message = createMessage(QStringLiteral("Activate"));
    message.setArguments({tabId});
    QDBusConnection::sessionBus().call(message); // asyncCall?
}

QDBusMessage TabsRunner::createMessage(const QString &method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasma.browser_integration"),
                                          QStringLiteral("/TabsRunner"),
                                          QStringLiteral("org.kde.plasma.browser_integration.TabsRunner"),
                                          method);
}

QMimeData *TabsRunner::mimeDataForMatch(const Plasma::QueryMatch &match)
{
    Q_UNUSED(match);
    // TODO return tab url or maybe for firefox a magic "dragging tab off a window" mime?
    return nullptr;
}

QList<QAction *> TabsRunner::actionsForMatch(const Plasma::QueryMatch &match)
{
    QList<QAction *> actions;

    const QVariantHash &tabData = match.data().toHash();

    const bool audible = tabData.value(QStringLiteral("audible")).toBool();
    const bool muted = tabData.value(QStringLiteral("muted")).toBool();

    if (audible) {
        if (muted) {
            actions << action(s_unmuteTab);
        } else {
            actions << action(s_muteTab);
        }
    }

    return actions;
}

K_EXPORT_PLASMA_RUNNER(browsertabs, TabsRunner)

#include "tabsrunner.moc"
