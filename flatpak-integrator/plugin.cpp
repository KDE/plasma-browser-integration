// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

#include <fcntl.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSocketNotifier>

#include <KDEDModule>
#include <KPluginFactory>

using namespace Qt::StringLiterals;

bool openAndMonitor(QFile *file, const QDBusUnixFileDescriptor &dbusFd, QFile::OpenMode mode, QProcess *host, QObject *parent)
{
    auto fd = fcntl(dbusFd.fileDescriptor(), F_DUPFD_CLOEXEC);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_CLOEXEC | O_SYNC | O_NONBLOCK); // For stdin particularly we want to be non-blocking and sync!
    if (fd == -1) {
        qWarning() << "Failed to dup fd" << dbusFd.fileDescriptor();
        return false;
    }

    if (!file->open(fd, mode | QIODevice::Unbuffered, QFileDevice::AutoCloseHandle)) {
        qWarning() << "Failed to open file descriptor" << fd;
        return false;
    }

    if (mode == QFile::WriteOnly) {
        // Don't need a notifier for writing
        return true;
    }

    auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, parent);
    QObject::connect(notifier, &QSocketNotifier::activated, parent, [parent, file, host](QSocketDescriptor, QSocketNotifier::Type) {
        auto data = file->readAll();
        if (data.isEmpty() && file->atEnd()) {
            qDebug() << "Socket has presumably been closed. Discarding integrator.";
            parent->deleteLater();
            return;
        }
        qDebug() << "stdin:" << data;
        host->write(data);
    });

    return true;
}

class Integrator : public QObject
{
    Q_OBJECT
public:
    Integrator(const QStringList &args,
               const QDBusUnixFileDescriptor &stdin,
               const QDBusUnixFileDescriptor &stdout,
               const QDBusUnixFileDescriptor &stderr,
               QObject *parent)
        : QObject(parent)
    {
        qDebug() << "Hello starting host" << args << stdin.fileDescriptor() << stdout.fileDescriptor() << stderr.fileDescriptor();

        auto host = new QProcess(this);
        host->setProgram(u"plasma-browser-integration-host"_s);
        host->setArguments(args);

        auto stdinFile = new QFile(this);

        auto stdoutFile = new QFile(this);
        connect(host, &QProcess::readyReadStandardOutput, this, [host, stdoutFile]() {
            auto data = host->readAllStandardOutput();
            qDebug() << "stdout:" << data;
            stdoutFile->write(data);
            stdoutFile->flush();
        });

        auto stderrFile = new QFile(this);
        connect(host, &QProcess::readyReadStandardError, this, [host, stderrFile]() {
            auto data = host->readAllStandardError();
            qDebug() << "stderr:" << data;
            stderrFile->write(data);
            stderrFile->flush();
        });

        if (!openAndMonitor(stdinFile, stdin, QIODevice::ReadOnly, host, this)) {
            qDebug() << "Failed to open stdin";
            return;
        }
        if (!openAndMonitor(stdoutFile, stdout, QIODevice::WriteOnly, host, this)) {
            qDebug() << "Failed to open stdout";
            return;
        }
        if (!openAndMonitor(stderrFile, stderr, QIODevice::WriteOnly, host, this)) {
            qDebug() << "Failed to open stderr";
            return;
        }

        qDebug() << "Starting host" << host->program() << host->arguments();
        host->start();
    }
};

class BrowserIntegrationFlatpakIntegrator : public KDEDModule
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.plasma.browser.integration.FlatpakIntegrator")

public:
    BrowserIntegrationFlatpakIntegrator(QObject *parent, const QList<QVariant> &)
        : KDEDModule(parent)
    {
        auto flatpak = new QProcess(this);
        connect(flatpak, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [flatpak](int, QProcess::ExitStatus) {
            flatpak->deleteLater();
        });
        flatpak->start(u"flatpak"_s, {u"override"_s, u"--user"_s, u"--talk-name=org.kde.plasma.browser.integration"_s, u"org.mozilla.firefox"_s});

        // Register on separate bus connection to avoid exposing other services to the sandbox
        auto connection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "org.kde.plasma.browser.integration"_L1);
        connection.registerService("org.kde.plasma.browser.integration"_L1);
        connection.registerObject("/org/kde/plasma/browser/integration"_L1, this, QDBusConnection::ExportAllSlots);

        createMessagingHost(); // always create integration regardless of firefox being installed so we can be ready for the browser
    }

public Q_SLOTS:
    QDBusObjectPath
    Link(const QStringList &args, const QDBusUnixFileDescriptor &stdin, const QDBusUnixFileDescriptor &stdout, const QDBusUnixFileDescriptor &stderr)
    {
        auto integrator = new Integrator(args, stdin, stdout, stderr, this);
        const QString integrationPath = "/org/kde/plasma/browser/integration/"_L1 + QString::number(reinterpret_cast<qint64>(integrator));
        return QDBusObjectPath(integrationPath);
    }

private:
    void createMessagingHost()
    {
        { // host wrapper
            QDir().mkpath(m_hostWrapperDir);
            if (QFile::exists(m_hostWrapperPath)) {
                QFile::remove(m_hostWrapperPath);
            }
            if (!QFile::copy(u":/data/flatpak-host-wrapper"_s, m_hostWrapperPath)) {
                qWarning() << "Failed to copy host wrapper";
                return;
            }
            QFile::setPermissions(m_hostWrapperPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        }

        { // hosts definition
            const QString extensionDefinitionDir = QDir::homePath() + "/.var/app/org.mozilla.firefox/.mozilla/native-messaging-hosts"_L1;
            const QString extensionDefinitionPath = extensionDefinitionDir + "/org.kde.plasma.browser_integration.json"_L1;

            QDir().mkpath(extensionDefinitionDir);
            QFile extensionDefinition(extensionDefinitionPath);
            if (!extensionDefinition.open(QIODevice::WriteOnly)) {
                qWarning() << "Failed to open extension definition file";
                return;
            }
            QJsonObject extensionDefinitionObject({
                {u"name"_s, u"org.kde.plasma.browser_integration"_s},
                {u"description"_s, u"Native connector for KDE Plasma"_s},
                {u"path"_s, m_hostWrapperPath},
                {u"type"_s, u"stdio"_s},
                {u"allowed_extensions"_s, QJsonArray({u"plasma-browser-integration@kde.org"_s})},
            });
            extensionDefinition.write(QJsonDocument(extensionDefinitionObject).toJson());
        }
    }

    const QString m_hostWrapperDir = QDir::homePath() + "/.var/app/org.mozilla.firefox"_L1;
    const QString m_hostWrapperPath = m_hostWrapperDir + "/plasma-browser-integration-host"_L1;
};

K_PLUGIN_FACTORY_WITH_JSON(BrowserIntegrationFlatpakIntegratorFactory,
                           "browserintegrationflatpakintegrator.json",
                           registerPlugin<BrowserIntegrationFlatpakIntegrator>();)

#include "plugin.moc"
