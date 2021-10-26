/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include <QFile>
#include <QJsonObject>
#include <QObject>

/*
 * This class is responsible for managing all stdout/stdin connections emitting JSON
 */
class Connection : public QObject
{
    Q_OBJECT
public:
    static Connection *self();
    void sendData(const QJsonObject &data);

Q_SIGNALS:
    void dataReceived(const QJsonObject &data);

private:
    Connection();
    ~Connection() override = default;
    void readData();
    QFile m_stdOut;
    QFile m_stdIn;
};
