/*
    Copyright (C) 2019 by Kai Uwe Broulik <kde@privat.broulik.de>

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

#include "abstractbrowserplugin.h"

class ItineraryPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT

public:
    explicit ItineraryPlugin(QObject *parent);
    ~ItineraryPlugin() override;

    QJsonObject status() const override;

    bool onLoad() override;
    bool onUnload() override;

    using AbstractBrowserPlugin::handleData;
    QJsonObject handleData(int serial, const QString &event, const QJsonObject &data) override;

private:
    void extract(int serial, const QJsonObject &data);
    void sendUrlToDevice(int serial, const QString &deviceId, const QUrl &url);

    static QUrl geoUrlFromJson(const QJsonObject &json);

    void checkSupported();

    bool m_supported = false;

    bool m_itineraryFound = false;
    bool m_icalHandlerFound = false;
    bool m_workbenchFound = false;

    QString m_geoHandlerName;

};
