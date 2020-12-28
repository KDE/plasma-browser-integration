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

#pragma once

#include "abstractkrunnerplugin.h"

#include <QDBusMessage>
#include <QMultiHash>

class TabsRunnerPlugin : public AbstractKRunnerPlugin
{
    Q_OBJECT

public:
    explicit TabsRunnerPlugin(QObject *parent);

    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

    // DBus API
    RemoteActions Actions() override;
    RemoteMatches Match(const QString &searchTerm) override;
    void Run(const QString &id, const QString &actionId) override;

private:
    QMultiHash<QString, QDBusMessage> m_requests;

};
