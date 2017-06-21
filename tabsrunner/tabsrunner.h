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

#pragma once

#include <KRunner/AbstractRunner>

#include <QDBusMessage>

class TabsRunner : public Plasma::AbstractRunner
{
    Q_OBJECT

public:
    TabsRunner(QObject *parent, const QVariantList &args);
    ~TabsRunner() override;

    void match(Plasma::RunnerContext &context) override;
    void run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &action) override;

protected slots:
    QMimeData *mimeDataForMatch(const Plasma::QueryMatch &match) override;
    QList<QAction *> actionsForMatch(const Plasma::QueryMatch &match) override;

private:
    static QDBusMessage createMessage(const QString &service, const QString &method);

    // accessed in match()
    QHash<QString /*dbus service name*/, QString /*browser (chrome, firefox, ..)*/> m_serviceToBrowser;

};
