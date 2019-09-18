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

#include "purposeplugin.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <KIO/MimetypeJob>

#include <Purpose/AlternativesModel>
#include <PurposeWidgets/Menu>

PurposePlugin::PurposePlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("purpose"), 1, parent)
{

}

PurposePlugin::~PurposePlugin()
{
    onUnload();
}

bool PurposePlugin::onUnload()
{
    m_menu.reset();
    return true;
}

QJsonObject PurposePlugin::handleData(int serial, const QString &event, const QJsonObject &data)
{
    if (event == QLatin1String("share")) {

        if (m_pendingReplySerial != -1 || (m_menu && m_menu->isVisible())) {
            return {
                {QStringLiteral("success"), false},
                {QStringLiteral("errorCode"), QStringLiteral("BUSY")}
            };
        }

        // store request serial for asynchronous reply
        m_pendingReplySerial = serial;

        const QJsonObject shareData = data.value(QStringLiteral("data")).toObject();

        const QString title = shareData.value(QStringLiteral("title")).toString();
        const QString text = shareData.value(QStringLiteral("text")).toString();
        const QString urlString = shareData.value(QStringLiteral("url")).toString();

        if (!m_menu) {
            m_menu.reset(new Purpose::Menu());
            m_menu->model()->setPluginType(QStringLiteral("Export"));

            connect(m_menu.data(), &QMenu::aboutToHide, this, [this] {
                if (!m_menu->activeAction()) {
                    sendPendingReply(false, {
                        {QStringLiteral("errorCode"), QStringLiteral("CANCELED")}
                    });
                }
            });

            connect(m_menu.data(), &Purpose::Menu::finished, this, [this](const QJsonObject &output, int errorCode, const QString &errorMessage) {
                if (errorCode) {
                    debug() << "Error:" << errorCode << errorMessage;

                    sendPendingReply(false, {
                        {QStringLiteral("errorCode"), errorCode},
                        {QStringLiteral("errorMessage"), errorMessage}
                    });
                    return;
                }

                const QString url = output.value(QStringLiteral("url")).toString();
                if (!url.isEmpty()) {
                    // Do this here rather than on the extension side to avoid having to request an additional permission after updating
                    QGuiApplication::clipboard()->setText(url);
                }

                debug() << "Finished:" << output;
                sendPendingReply(true, {
                    {QStringLiteral("response"), output}
                });
            });
        }

        QJsonObject shareJson;

        if (!title.isEmpty()) {
            shareJson.insert(QStringLiteral("title"), title);
        }

        QJsonArray urls;
        if (!urlString.isEmpty()) {
            urls.append(urlString);
        }
        // Sends even text as URL...
        if (!text.isEmpty()) {
            urls.append(text);
        }

        if (!urls.isEmpty()) {
            shareJson.insert(QStringLiteral("urls"), urls);
        }

        if (!text.isEmpty()) {
            showShareMenu(shareJson, QStringLiteral("text/plain"));
            return {};
        }

        if (!urls.isEmpty()) {
            auto *mimeJob = KIO::mimetype(QUrl(urlString), KIO::HideProgressInfo);
            connect(mimeJob, &KJob::finished, this, [this, mimeJob, shareJson] {
                showShareMenu(shareJson, mimeJob->mimetype());
            });
            return {};
        }

        // navigator.share({title: "foo"}) is valid but makes no sense
        // and we also cannot share via Purpose without "urls"
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("errorCode"), QStringLiteral("INVALID_ARGUMENT")}
        };
    }

    return {};
}

void PurposePlugin::sendPendingReply(bool success, const QJsonObject &data)
{
    QJsonObject reply = data;
    reply.insert(QStringLiteral("success"), success);

    sendReply(m_pendingReplySerial, reply);
    m_pendingReplySerial = -1;
}

void PurposePlugin::showShareMenu(const QJsonObject &data, const QString &mimeType)
{
    QJsonObject shareData = data;

    if (!mimeType.isEmpty() && mimeType != QLatin1String("application/octet-stream")) {
        shareData.insert(QStringLiteral("mimeType"), mimeType);
    } else {
        shareData.insert(QStringLiteral("mimeType"), QStringLiteral("*/*"));
    }

    debug() << "Share mime type" << mimeType << "with data" << data;

    m_menu->model()->setInputData(shareData);
    m_menu->reload();

    m_menu->popup(QCursor::pos());
}
