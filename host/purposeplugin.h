/*
    SPDX-FileCopyrightText: 2019 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include "abstractbrowserplugin.h"

#include <QPointer>
#include <QScopedPointer>
#include <QString>
#include <QUrl>

namespace Purpose {
class Menu;
}

class PurposePlugin : public AbstractBrowserPlugin
{
    Q_OBJECT

public:
    explicit PurposePlugin(QObject *parent);
    ~PurposePlugin() override;

    bool onUnload() override;

    using AbstractBrowserPlugin::handleData;
    QJsonObject handleData(int serial, const QString &event, const QJsonObject &data) override;

private:
    void showShareMenu(const QJsonObject &data, const QString &mimeType = QString());

    void sendPendingReply(bool success, const QJsonObject &data);

    QScopedPointer<Purpose::Menu> m_menu;

    int m_pendingReplySerial = -1;
};
