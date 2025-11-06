/*
    SPDX-FileCopyrightText: 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: MIT
*/

#include <QApplication>
#include <QDBusConnection>
#include <QDebug>

#include <KAboutData>
#include <KCrash>

#include "abstractbrowserplugin.h"
#include "connection.h"
#include "pluginmanager.h"

#include "downloadplugin.h"
#include "historyrunnerplugin.h"
#include "kdeconnectplugin.h"
#include "mprisplugin.h"
#include "purposeplugin.h"
#include "settings.h"
#include "tabsrunnerplugin.h"

#include <version.h>

void msgHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QJsonObject data;
    data[QStringLiteral("subsystem")] = QStringLiteral("debug");
    switch (type) {
    case QtDebugMsg:
    case QtInfoMsg:
        data[QStringLiteral("action")] = QStringLiteral("debug");
        break;
    default:
        data[QStringLiteral("action")] = QStringLiteral("warning");
    }

    QJsonObject payload{{QStringLiteral("message"), msg}};

    // NOTE For compatibility they are still action debug/warning
    switch (type) {
    case QtInfoMsg:
        payload.insert(QStringLiteral("severity"), QStringLiteral("info"));
        break;
    case QtCriticalMsg:
        payload.insert(QStringLiteral("severity"), QStringLiteral("critical"));
        break;
    case QtFatalMsg:
        payload.insert(QStringLiteral("severity"), QStringLiteral("fatal"));
        break;
    default:
        break;
    }

    if (context.file) {
        payload.insert(QStringLiteral("file"), QString::fromUtf8(context.file));
    }
    if (context.line) {
        payload.insert(QStringLiteral("line"), context.line);
    }
    if (context.function) {
        payload.insert(QStringLiteral("function"), QString::fromUtf8(context.function));
    }
    if (context.category) {
        payload.insert(QStringLiteral("category"), QString::fromUtf8(context.category));
    }

    data[QStringLiteral("payload")] = payload;

    Connection::self()->sendData(data);
}

int main(int argc, char *argv[])
{
    // otherwise when logging out, session manager will ask the host to quit
    // (it's a "regular X app" after all) and then the browser will complain
    QCoreApplication::setAttribute(Qt::AA_DisableSessionManager);
    QApplication a(argc, argv);
    // otherwise will close when download job finishes
    a.setQuitOnLastWindowClosed(false);
    a.setQuitLockEnabled(false);
    // applicationName etc will be set in Settings once the browser identifies to us

    qInstallMessageHandler(msgHandler);

    KAboutData about(QStringLiteral("plasma-browser-integration-host"), QString(), QStringLiteral(PBI_VERSION_STRING));
    KAboutData::setApplicationData(about);

    KCrash::initialize();

    // NOTE if you add a new plugin here, make sure to adjust the
    // "DEFAULT_EXTENSION_SETTINGS" in constants.js or else it won't
    // even bother loading your shiny new plugin!

    PluginManager::self().addPlugin(&Settings::self());
    PluginManager::self().addPlugin(new KDEConnectPlugin(&a));
    PluginManager::self().addPlugin(new DownloadPlugin(&a));
    PluginManager::self().addPlugin(new TabsRunnerPlugin(&a));
    PluginManager::self().addPlugin(new HistoryRunnerPlugin(&a));
    PluginManager::self().addPlugin(new MPrisPlugin(&a));
    PluginManager::self().addPlugin(new PurposePlugin(&a));

    // TODO make this prettier, also prevent unloading them at any cost
    PluginManager::self().loadPlugin(&Settings::self());

    QString serviceName = QStringLiteral("org.kde.plasma.browser_integration");
    if (!QDBusConnection::sessionBus().registerService(serviceName)) {
        // now try appending PID in case multiple hosts are running
        serviceName.append(QLatin1String("-")).append(QString::number(QCoreApplication::applicationPid()));
        if (!QDBusConnection::sessionBus().registerService(serviceName)) {
            qWarning() << "Failed to register DBus service name" << serviceName;
        }
    }

    return a.exec();
}
