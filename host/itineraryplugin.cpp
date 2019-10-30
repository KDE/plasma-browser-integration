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

#include "itineraryplugin.h"

#include <QMetaEnum>
#include <QMimeDatabase>

#include "itineraryextractorjob.h"

ItineraryPlugin::ItineraryPlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("itinerary"), 1, parent)
    , m_supported(ItineraryExtractorJob::isSupported())
{

}

ItineraryPlugin::~ItineraryPlugin()
{

}

QJsonObject ItineraryPlugin::status() const
{
    return {
        {QStringLiteral("extractorFound"), m_supported}
    };
}

bool ItineraryPlugin::onLoad()
{
    // Check again on load, so reloading the plugin updates
    m_supported = ItineraryExtractorJob::isSupported();
    return m_supported;
}

bool ItineraryPlugin::onUnload()
{
    return true;
}

QJsonObject ItineraryPlugin::handleData(int serial, const QString &event, const QJsonObject &data)
{
    if (event == QLatin1String("extract")) {

        const QString dataString = data.value(QStringLiteral("data")).toString();

        QByteArray inputData;
        if (dataString.startsWith(QLatin1String("data:"))) {
            const int b64start = dataString.indexOf(QLatin1Char(','));
            QByteArray b64 = dataString.rightRef(dataString.size() - b64start - 1).toLatin1();
            inputData = QByteArray::fromBase64(b64);
        } else {
            inputData = dataString.toUtf8();
        }

        auto *job = new ItineraryExtractorJob(inputData);

        const QString type = data.value(QStringLiteral("type")).toString();

        const QMetaEnum me = QMetaEnum::fromType<ItineraryExtractorJob::InputType>();
        const auto inputType = static_cast<ItineraryExtractorJob::InputType>(me.keyToValue(qUtf8Printable(type)));
        job->setInputType(inputType);

        connect(job, &KJob::result, this, [this, job, serial] {
            if (job->error()) {
                sendReply(serial, {
                    {QStringLiteral("success"), false},
                    {QStringLiteral("errorCode"), job->error()},
                    {QStringLiteral("errorMessage"), job->errorText()}
                });
                return;
            }

            sendReply(serial, {
                {QStringLiteral("success"), true},
                {QStringLiteral("data"), job->extractedData()}
            });
        });

        job->start();

    } else if (event == QLatin1String("run")) {

        // TODO open file in Itinerary

    }

    return {};
}
