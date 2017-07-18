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

#include "connection.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>

#include <QDebug>
#include <unistd.h>
#include <poll.h>

Connection::Connection() :
    QObject()
{
    m_stdOut.open(STDOUT_FILENO, QIODevice::WriteOnly);
    m_stdIn.open(STDIN_FILENO, QIODevice::ReadOnly | QIODevice::Unbuffered);

    auto notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &Connection::readData);
}

void Connection::sendData(const QJsonObject &data)
{
    const QByteArray rawData = QJsonDocument(data).toJson(QJsonDocument::Compact);
    //note, don't use QDataStream as we need to control the binary format used
    quint32 len = rawData.count();
    m_stdOut.write((char*)&len, sizeof(len));
    m_stdOut.write(rawData);
    m_stdOut.flush();
}

Connection* Connection::self()
{
    static Connection *s = 0;
    if (!s) {
        s = new Connection();
    }
    return s;
}

void Connection::readData()
{
    /* Qt does not recognize POLLHUP as an error and
     * as that flag never gets cleared, we enter an
     * infinite busy loop polling STDIN.
     * So we need to check for this condition ourselves
     * and exit. */

    struct pollfd poll_stdin = {
        .fd = STDIN_FILENO,
        .events = POLLHUP,
        .revents = 0
    };
    if (poll (&poll_stdin, 1, 0) != 0) {
        // STDIN has HUP/ERR/NVAL condition
        qApp->exit(0);
        return;
    }

    m_stdIn.startTransaction();
    quint32 length = 0;
    auto rc = m_stdIn.read((char*)(&length), sizeof(quint32));
    if (rc == -1) {
        m_stdIn.rollbackTransaction();
        return;
    }

    QByteArray data = m_stdIn.read(length);
    if (data.length() != length) {
        m_stdIn.rollbackTransaction();
        return;
    }

    if (data.isEmpty()) {
        m_stdIn.rollbackTransaction();
        return;
    }

    m_stdIn.commitTransaction();
    const QJsonObject json = QJsonDocument::fromJson(data).object();
    emit dataReceived(json);
}
