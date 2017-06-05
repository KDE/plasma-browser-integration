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

#include <QObject>
#include <QJsonObject>
#include <QDebug>

class AbstractBrowserPlugin : public QObject
{
    Q_OBJECT
public:
    ~AbstractBrowserPlugin() = default;
    QString subsystem() const;
    virtual void handleData(const QString &event, const QJsonObject &data);

    virtual bool onLoad();
    virtual bool onUnload();
    virtual void onSettingsChanged(const QJsonObject &newSettings);

    bool isLoaded() const;
    // FIXME this should not be public but we need to change it from main.cpp
    void setLoaded(bool loaded);

protected:
    /*
     * @arg subsystemId
     * The name of the plugin. This will be used for the "subsystem" parameter for all data sent
     *
     * @arg protocolVersion
     * As the browser extension will be shipped separately to the native plugin a user could have incompatiable setups
     * Here we inform the browser of the protocol used so if we do ever changed the native API we can at least detect it on the JS side and handle it
     */
    AbstractBrowserPlugin(const QString &subsystemId, int protocolVersion, QObject *parent);
    void sendData(const QString &action, const QJsonObject &payload = QJsonObject());
    QDebug debug() const;

    QJsonObject settings() const;

private:
    QString m_subsystem;
    bool m_loaded = false;
};
