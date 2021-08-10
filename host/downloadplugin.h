/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include "abstractbrowserplugin.h"

#include "downloadjob.h"
#include <QHash>

class KUiServerV2JobTracker;

class DownloadPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT
public:
    explicit DownloadPlugin(QObject *parent);
    bool onLoad() override;
    bool onUnload() override;
    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

private:
    KUiServerV2JobTracker *m_tracker;
    QHash<int, DownloadJob *> m_jobs;
};
