/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include <QObject>
#include <QFile>
#include <QJsonObject>

/*
 * This class is responsible for managing all stdout/stdin connections emitting JSON
 */
class Connection : public QObject
{
    Q_OBJECT
public:
    static Connection* self();
    void sendData(const QJsonObject &data);

Q_SIGNALS:
    void dataReceived(const QJsonObject &data);

private:
    Connection();
    ~Connection() = default;
    void readData();
    QFile m_stdOut;
    QFile m_stdIn;
};
