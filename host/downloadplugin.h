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
class KInhibitionJobTracker;

class DownloadPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT
public:
    explicit DownloadPlugin(QObject *parent);

    void onSettingsChanged(const QJsonObject &settings) override;
    bool onLoad() override;
    bool onUnload() override;
    using AbstractBrowserPlugin::handleData;
    void handleData(const QString &event, const QJsonObject &data) override;

private:
    void registerJobInhibition(KJob *job);

    KUiServerV2JobTracker *const m_uiServerTracker;
    KInhibitionJobTracker *m_inhibitionTracker = nullptr;
    QHash<int, DownloadJob *> m_jobs;
};
