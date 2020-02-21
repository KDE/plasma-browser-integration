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

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QUrl>
#include <QProcess>
#include <QTemporaryFile>
#include <QUrlQuery>

#include <KService>
#include <KMimeTypeTrader>

#include "itineraryextractorjob.h"
#include "kdeconnectjob.h"

static const QString s_itineraryDesktopEntry = QStringLiteral("org.kde.itinerary");
static const QString s_workbenchDesktopEntry = QStringLiteral("org.kde.kitinerary-workbench");

ItineraryPlugin::ItineraryPlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("itinerary"), 1, parent)
{
    checkSupported();
}

ItineraryPlugin::~ItineraryPlugin()
{

}

void ItineraryPlugin::checkSupported()
{
    m_supported = ItineraryExtractorJob::isSupported();

    m_itineraryFound = KService::serviceByDesktopName(s_itineraryDesktopEntry);

    m_icalHandlerFound = false;
    // Check if there is a dedicated handler for ical files which isn't Itinerary and not a text editor
    const auto icalHandlers = KMimeTypeTrader::self()->query(QStringLiteral("text/calendar"));
    for (const KService::Ptr &service : icalHandlers) {
        // We can handle structured data, don't bother sending us calendar files
        // FIXME re-enable in release version
        /*if (service->desktopEntryName() == s_itineraryDesktopEntry
                || service->desktopEntryName() == s_workbenchDesktopEntry) {
            continue;
        }*/

        // Skip out all sorts of text editors
        if (service->categories().contains(QLatin1String("TextEditor"))
                || service->categories().contains(QLatin1String("WordProcessor"))) {
            continue;
        }

        // If it can only handle plain text files, we're not interested
        if (service->mimeTypes().count() == 1 && service->mimeTypes().first() == QLatin1String("text/plain")) {
            continue;
        }

        m_icalHandlerFound = true;
        break;
    }

    m_geoHandlerName.clear();
    const auto geoHandlers = KMimeTypeTrader::self()->query(QStringLiteral("x-scheme-handler/geo"));
    for (const KService::Ptr &service : geoHandlers) {
        m_geoHandlerName = service->name();
        // FIXME what if we have multiple? Check KRun code what it does
        break;
    }


    m_workbenchFound = KService::serviceByDesktopName(s_workbenchDesktopEntry);
}

QJsonObject ItineraryPlugin::status() const
{
    return {
        {QStringLiteral("extractorFound"), m_supported},
        {QStringLiteral("itineraryFound"), m_itineraryFound},
        {QStringLiteral("icalHandlerFound"), m_icalHandlerFound},
        {QStringLiteral("geoHandlerName"), m_geoHandlerName},
        {QStringLiteral("workbenchFound"), m_workbenchFound}
    };
}

bool ItineraryPlugin::onLoad()
{
    checkSupported();
    return m_supported;
}

bool ItineraryPlugin::onUnload()
{
    return true;
}

QUrl ItineraryPlugin::geoUrlFromJson(const QJsonObject &json)
{
    QUrl url;

    const QString latitudeString = json.value(QStringLiteral("latitude")).toString();
    const QString longitudeString = json.value(QStringLiteral("longitude")).toString();

    bool latitudeOk = false;
    bool longitudeOk = false;
    const auto latitude = latitudeString.toDouble(&latitudeOk);
    const auto longitude = longitudeString.toDouble(&longitudeOk);

    const QString address = json.value(QStringLiteral("address")).toString();

    if (latitudeOk && longitudeOk) {
        url.setPath(QStringLiteral("%1,%2").arg(QString::number(latitude, 'f'), QString::number(longitude, 'f')));
    } else if (!address.isEmpty()) {
        // should we still keep the query if we know the geo coordinates?
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("q"), address);
        url.setQuery(query);
    } else { // neither geo nor query, invalid
        return url;
    }

    url.setScheme(QStringLiteral("geo")); // set last so the url remains invalid above

    return url;
}

void ItineraryPlugin::extract(int serial, const QJsonObject &data)
{
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
            {QStringLiteral("data"), QJsonDocument::fromJson(job->extractedData()).array()}
        });
    });

    job->start();
}

void ItineraryPlugin::sendUrlToDevice(int serial, const QString &deviceId, const QUrl &url)
{
    auto *job = new KdeConnectJob(deviceId, QStringLiteral("share"), QStringLiteral("shareUrl"));
    job->setArguments({url.toString()});
    connect(job, &KdeConnectJob::finished, this, [this, job, serial] {
        if (job->error()) {
            sendReply(serial, {
                {QStringLiteral("success"), false},
                {QStringLiteral("errorCode"), job->error()},
                {QStringLiteral("errorMessage"), job->errorText()}
            });
            return;
        }

        sendReply(serial, {
            {QStringLiteral("success"), true}
        });
    });
    job->start();
}

QJsonObject ItineraryPlugin::handleData(int serial, const QString &event, const QJsonObject &data)
{
    if (event == QLatin1String("extract")) {
        extract(serial, data);
    } else if (event == QLatin1String("openLocation")
        || event == QLatin1String("sendLocationToDevice")) {

        const QUrl geoUrl = geoUrlFromJson(data);
        if (!geoUrl.isValid()) {
            return { {QStringLiteral("success"), false} };
        }

        if (event == QLatin1String("openLocation")) {
            QDesktopServices::openUrl(geoUrl);
            return { {QStringLiteral("success"), true} };
        }

        Q_ASSERT(event == QLatin1String("sendLocationToDevice"));
        const QString deviceId = data.value(QStringLiteral("deviceId")).toString();
        sendUrlToDevice(serial, deviceId, geoUrl);

    } else if (event == QLatin1String("callOnDevice")) {

        const QString deviceId = data.value(QStringLiteral("deviceId")).toString();

        QUrl telUrl;
        telUrl.setScheme(QStringLiteral("tel"));
        telUrl.setPath(data.value(QStringLiteral("phoneNumber")).toString());
        sendUrlToDevice(serial, deviceId, telUrl);

    } else if (event == QLatin1String("addToCalendar")) {

        const QByteArray jsonData = QJsonDocument(data.value(QStringLiteral("data")).toArray()).toJson(QJsonDocument::Compact);

        // Basically send the JSON we got back to the extractor but ask for ICAL this time
        auto *job = new ItineraryExtractorJob(jsonData);
        // TODO document in itinerary-extractor that it supports that too and adjust the job to have it
        //job->setInputType(ItineraryExtractorJob::InputType::JsonLd);
        job->setOutputType(ItineraryExtractorJob::OutputType::ICal);

        connect(job, &KJob::result, this, [this, job, serial] {
            if (job->error()) {
                sendReply(serial, {
                    {QStringLiteral("success"), false},
                    {QStringLiteral("errorCode"), job->error()},
                    {QStringLiteral("errorMessage"), job->errorText()}
                });
                return;
            }

            QFile tempFile(QStringLiteral("/tmp/itinerary-pbi.ics"));
            // FIXME why does this fail with Permission denied error?!
            // Is it because I cannot open for reading for some browser sandbox?
            //QTemporaryFile tempFile(QStringLiteral("itinerary-XXXXXX.ics"));

            if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                warning() << "Failed to open temporary file for writing temporary calendar file";
                sendReply(serial, {
                    {QStringLiteral("success"), false},
                    {QStringLiteral("errorCode"), tempFile.error()},
                    {QStringLiteral("errorMessage"), tempFile.errorString()}
                });
                return;
            }

            //tempFile.setAutoRemove(false);
            tempFile.write(job->extractedData());
            tempFile.close();

            QDesktopServices::openUrl(QUrl::fromLocalFile(tempFile.fileName()));
            sendReply(serial, {
                {QStringLiteral("success"), true}
            });
        });

        job->start();

    } else if (event == QLatin1String("sendToDevice")) {

        const QJsonArray jsonData = data.value(QStringLiteral("data")).toArray();
        const QString deviceId = data.value(QStringLiteral("deviceId")).toString();

        // FIXME figure out a way to use the KItinerary::File without
        // linking it. Just reimplementing/copying it?
        // or have a kitinerary-packager
        warning() << "sendToDevice NOT IMPLEMENTED YET";

    } else if (event == QLatin1String("openInItinerary")
               || event == QLatin1String("openInWorkbench")) {

        const QString urlString = data.value(QStringLiteral("url")).toString();

        if (!QUrl(urlString, QUrl::StrictMode).isValid()) {
            warning() << "Cannot open invalid url" << urlString;
            return { {QStringLiteral("success"), false} };
        }

        // FIXME use KService exec lookup? Don't want to use KRun so I don't pull in KIOWidgets, though
        bool ok = false;
        if (event == QLatin1String("openInItinerary")) {
            // TODO have itinerary return with a proper exit code when it failed to load or extract or something?
            ok = QProcess::startDetached(QStringLiteral("itinerary"), { urlString });
        } else if (event == QLatin1String("openInWorkbench")) {
            ok = QProcess::startDetached(QStringLiteral("kitinerary-workbench"), {
                // FIXME what if we display a PDF?
                QStringLiteral("-t"), QStringLiteral("html"),
                urlString
            });
        }

        return { {QStringLiteral("success"), ok} };

    }

    return {};
}
