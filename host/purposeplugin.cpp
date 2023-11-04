/*
    SPDX-FileCopyrightText: 2019 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: MIT
*/

#include "purposeplugin.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <KIO/MimeTypeFinderJob>

#include <Purpose/AlternativesModel>
#include <Purpose/Menu>

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
                {QStringLiteral("errorCode"), QStringLiteral("BUSY")},
            };
        }

        // store request serial for asynchronous reply
        m_pendingReplySerial = serial;

        const QJsonObject shareData = data.value(QLatin1String("data")).toObject();

        QString title = shareData.value(QLatin1String("title")).toString();
        const QString text = shareData.value(QLatin1String("text")).toString();
        const QString urlString = shareData.value(QLatin1String("url")).toString();

        // Purpose ShareUrl plug-in type mandates a title.
        if (title.isEmpty()) {
            title = urlString;
        }

        if (!m_menu) {
            m_menu.reset(new Purpose::Menu());
            m_menu->model()->setPluginType(QStringLiteral("ShareUrl"));

            connect(m_menu.data(), &QMenu::aboutToShow, this, [this] {
                m_menu->setProperty("actionInvoked", false);
            });

            connect(m_menu.data(), &QMenu::aboutToHide, this, [this] {
                // aboutToHide is emitted before an action is triggered and activeAction() is
                // the action currently hovered. This means we can't properly tell that the prompt
                // got canceled, when hovering an action and then hitting Escape to close the menu.
                // Hence delaying this and checking if an action got invoked :(

                QMetaObject::invokeMethod(
                    this,
                    [this] {
                        if (!m_menu->property("actionInvoked").toBool()) {
                            sendPendingReply(false,
                                             {
                                                 {QStringLiteral("errorCode"), QStringLiteral("CANCELED")},
                                             });
                        }
                    },
                    Qt::QueuedConnection);
            });

            connect(m_menu.data(), &QMenu::triggered, this, [this] {
                m_menu->setProperty("actionInvoked", true);
            });

            connect(m_menu.data(), &Purpose::Menu::finished, this, [this](const QJsonObject &output, int errorCode, const QString &errorMessage) {
                if (errorCode) {
                    debug() << "Error:" << errorCode << errorMessage;

                    sendPendingReply(false,
                                     {
                                         {QStringLiteral("errorCode"), errorCode},
                                         {QStringLiteral("errorMessage"), errorMessage},
                                     });
                    return;
                }

                const QString url = output.value(QLatin1String("url")).toString();
                if (!url.isEmpty()) {
                    // Do this here rather than on the extension side to avoid having to request an additional permission after updating
                    QGuiApplication::clipboard()->setText(url);
                }

                debug() << "Finished:" << output;
                sendPendingReply(true, {{QStringLiteral("response"), output}});
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
            auto *mimeJob = new KIO::MimeTypeFinderJob(QUrl(urlString));
            mimeJob->setAuthenticationPromptEnabled(false);
            connect(mimeJob, &KIO::MimeTypeFinderJob::result, this, [this, mimeJob, shareJson] {
                showShareMenu(shareJson, mimeJob->mimeType());
            });
            mimeJob->start();
            return {};
        }

        // navigator.share({title: "foo"}) is valid but makes no sense
        // and we also cannot share via Purpose without "urls"
        m_pendingReplySerial = -1;
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("errorCode"), QStringLiteral("INVALID_ARGUMENT")},
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

#include "moc_purposeplugin.cpp"
