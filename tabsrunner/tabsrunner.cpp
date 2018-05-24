/*
 *   Copyright (C) 2017 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 3 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tabsrunner.h"

#include <QMimeData>

#include <QUrl>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
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

    // first look for all running hosts, there can be multiple browsers running
    QDBusReply<QStringList> servicesReply = QDBusConnection::sessionBus().interface()->registeredServiceNames();
    QStringList services;
    if (servicesReply.isValid()) {
        services = servicesReply.value();
    }

    for (const QString &service: services) {
        if (!service.startsWith(QLatin1String("org.kde.plasma.browser_integration"))) {
            continue;
        }

        QString browser = m_serviceToBrowser.value(service);
        if (browser.isEmpty()) { // now ask what browser we're dealing with
            // FIXME can we use our dbus xml for this?
            QDBusMessage message = QDBusMessage::createMethodCall(service,
                                               QStringLiteral("/Settings"),
                                               QStringLiteral("org.freedesktop.DBus.Properties"),
                                               QStringLiteral("Get"));
            message.setArguments({
                QStringLiteral("org.kde.plasma.browser_integration.Settings"),
                QStringLiteral("Environment")
            });

            QDBusMessage reply = QDBusConnection::sessionBus().call(message);

            if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().count() != 1) {
                continue;
            }

            // what a long tail of calls...
            browser = reply.arguments().at(0).value<QDBusVariant>().variant().toString();
            m_serviceToBrowser.insert(service, browser);
        }

        QDBusMessage message =
            QDBusMessage::createMethodCall(service,
                                           QStringLiteral("/TabsRunner"),
                                           QStringLiteral("org.kde.plasma.browser_integration.TabsRunner"),
                                           QStringLiteral("GetTabs")
            );

        QDBusMessage reply = QDBusConnection::sessionBus().call(message);

        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().length() != 1) {
            continue;
        }

        QList<Plasma::QueryMatch> matches;

        auto arg = reply.arguments().at(0).value<QDBusArgument>();
        auto tabvs = qdbus_cast<QList<QVariant>>(arg);

        for (const QVariant &tabv : tabvs)
        {
            auto tab = qdbus_cast<QVariantHash>(tabv.value<QDBusArgument>());

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
                {QStringLiteral("service"), service},
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

            QString iconName;
            QIcon icon;

            const QString favIconData = tab.value(QStringLiteral("favIconData")).toString();
            const int b64start = favIconData.indexOf(',');
            if (b64start != -1) {
                QByteArray b64 = favIconData.rightRef(favIconData.size() - b64start - 1).toLatin1();
                QByteArray data = QByteArray::fromBase64(b64);
                QPixmap pixmap;
                if (pixmap.loadFromData(data))
                   icon = QIcon(pixmap);
            }

            if (icon.isNull()) {
                if (incognito) {
                    iconName = QStringLiteral("face-smirk");// TODO QStringLiteral("incognito");
                } else if (browser == QLatin1String("chrome")) {
                    iconName = QStringLiteral("google-chrome");
                } else if (browser == QLatin1String("chromium")) {
                    iconName = QStringLiteral("chromium-browser");
                } else if (browser == QLatin1String("firefox")) {
                    iconName = QStringLiteral("firefox");
                } else if (browser == QLatin1String("opera")) {
                    iconName = QStringLiteral("opera");
                } else if (browser == QLatin1String("vivaldi")) {
                    iconName = QStringLiteral("vivaldi");
                }
            }

            if (audible) {
                if (muted) {
                    iconName = QStringLiteral("audio-volume-muted");
                } else {
                    iconName = QStringLiteral("audio-volume-high");
                }
            }

            if (!iconName.isEmpty()) {
                match.setIconName(iconName);
            } else if(!icon.isNull()) {
                match.setIcon(icon);
            }

            matches << match;
        }

        context.addMatches(matches);
    }
}

void TabsRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context);

    const QVariantHash &tabData = match.data().toHash();

    const QString &service = tabData.value(QStringLiteral("service")).toString();
    const int tabId = tabData.value(QStringLiteral("tabId")).toInt();

    if (match.selectedAction() == action(s_unmuteTab)) {
        QDBusMessage message = createMessage(service, QStringLiteral("SetMuted"));
        message.setArguments({tabId, false});
        QDBusConnection::sessionBus().call(message); // asyncCall?
        return;
    }

    if (match.selectedAction() == action(s_muteTab)) {
        QDBusMessage message = createMessage(service, QStringLiteral("SetMuted"));
        message.setArguments({tabId, true});
        QDBusConnection::sessionBus().call(message); // asyncCall?
        return;
    }

    QDBusMessage message = createMessage(service, QStringLiteral("Activate"));
    message.setArguments({tabId});
    QDBusConnection::sessionBus().call(message); // asyncCall?
}

QDBusMessage TabsRunner::createMessage(const QString &service, const QString &method)
{
    return QDBusMessage::createMethodCall(service,
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
