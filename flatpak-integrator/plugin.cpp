// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

#include <fcntl.h>
#include <sys/stat.h>

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

#include "debug.h"

using namespace Qt::StringLiterals;

bool openAndMonitor(QFile *file, const QDBusUnixFileDescriptor &dbusFd, QFile::OpenMode mode, QProcess *host, QObject *parent)
{
    auto fd = fcntl(dbusFd.fileDescriptor(), F_DUPFD_CLOEXEC);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_CLOEXEC | O_SYNC | O_NONBLOCK); // For stdin particularly we want to be non-blocking and sync!
    if (fd == -1) {
        qCWarning(INTEGRATOR) << "Failed to dup fd" << dbusFd.fileDescriptor();
        return false;
    }

    if (!file->open(fd, mode | QIODevice::Unbuffered, QFileDevice::AutoCloseHandle)) {
        qCWarning(INTEGRATOR) << "Failed to open file descriptor" << fd;
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
            qCDebug(INTEGRATOR) << "Socket has presumably been closed. Discarding integrator.";
            parent->deleteLater();
            return;
        }
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
        qCDebug(INTEGRATOR) << "Hello starting host" << args << stdin.fileDescriptor() << stdout.fileDescriptor() << stderr.fileDescriptor();

        auto host = new QProcess(this);
        host->setProgram(u"plasma-browser-integration-host"_s);
        host->setArguments(args);

        auto stdinFile = new QFile(this);

        auto stdoutFile = new QFile(this);
        connect(host, &QProcess::readyReadStandardOutput, this, [host, stdoutFile]() {
            auto data = host->readAllStandardOutput();
            stdoutFile->write(data);
            stdoutFile->flush();
        });

        auto stderrFile = new QFile(this);
        connect(host, &QProcess::readyReadStandardError, this, [host, stderrFile]() {
            auto data = host->readAllStandardError();
            stderrFile->write(data);
            stderrFile->flush();
        });

        if (!openAndMonitor(stdinFile, stdin, QIODevice::ReadOnly, host, this)) {
            qCDebug(INTEGRATOR) << "Failed to open stdin";
            return;
        }
        if (!openAndMonitor(stdoutFile, stdout, QIODevice::WriteOnly, host, this)) {
            qCDebug(INTEGRATOR) << "Failed to open stdout";
            return;
        }
        if (!openAndMonitor(stderrFile, stderr, QIODevice::WriteOnly, host, this)) {
            qCDebug(INTEGRATOR) << "Failed to open stderr";
            return;
        }

        qCDebug(INTEGRATOR) << "Starting host" << host->program() << host->arguments();
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
        connect(flatpak, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [flatpak](int, QProcess::ExitStatus) {
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
    void Link(const QStringList &args, const QDBusUnixFileDescriptor &stdin, const QDBusUnixFileDescriptor &stdout, const QDBusUnixFileDescriptor &stderr)
    {
        // Gets deleted either on exit or when the remote disconnects from the sockets (see openAndMonitor)
        new Integrator(args, stdin, stdout, stderr, this);
    }

private:
    char *safe_strerror(int error)
    {
        constexpr auto maxBufferSize = 1024;
        thread_local std::array<char, maxBufferSize> buffer;
        return strerror_r(error, buffer.data(), buffer.size());
    }

    int openNoSymlinks(const std::filesystem::path &path, int flags, mode_t mode = 0)
    {
        int dirfd = -1;
        const auto closeDirFd = qScopeGuard([&dirfd] { // by reference so we close the last open dirfd
            close(dirfd);
        });
        for (const auto &segment : path) {
            int newDirFd = openat(dirfd, segment.c_str(), O_PATH | O_NOFOLLOW | O_CLOEXEC);
            const auto closeDirFd = qScopeGuard([dirfd] { // copy so we close this dirfd, not the new one
                close(dirfd);
            });
            if (newDirFd == -1) {
                return -1;
            }
            dirfd = newDirFd;
        }
        return openat(dirfd, ".", flags | O_NOFOLLOW | O_CLOEXEC, mode);
    }

    void createMessagingHost()
    {
        QDir().mkpath(m_hostWrapperDir);

        auto hostWrapperDirFd = openNoSymlinks(qUtf8Printable(m_hostWrapperDir), O_PATH);
        const auto closeHostWrapperDirFd = qScopeGuard([hostWrapperDirFd]() {
            close(hostWrapperDirFd);
        });
        if (hostWrapperDirFd == -1) {
            auto err = errno;
            qCWarning(INTEGRATOR) << "Failed to open hostWrapper directory." << m_hostWrapperDir << ":" << safe_strerror(err);
            return;
        }

        { // host wrapper
            auto hostWrapperFd = openat(hostWrapperDirFd, qUtf8Printable(m_hostWrapperName), O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC | O_NOFOLLOW, S_IRWXU);
            auto closeHostWrapperFd = qScopeGuard([hostWrapperFd]() {
                close(hostWrapperFd);
            });
            if (hostWrapperFd == -1) {
                auto err = errno;
                qCWarning(INTEGRATOR) << "Failed to open host wrapper file" << m_hostWrapperName << ":" << safe_strerror(err);
                return;
            }

            QFile dataFile(u":/data/flatpak-host-wrapper"_s);
            if (!dataFile.open(QIODevice::ReadOnly)) {
                qCWarning(INTEGRATOR) << "Failed to open host wrapper data";
                return;
            }
            const auto data = dataFile.readAll();
            if (data.size() <= 0) {
                qCWarning(INTEGRATOR) << "Failed to read host wrapper data";
                return;
            }

            QFile hostWrapperFile;
            if (!hostWrapperFile.open(hostWrapperFd, QIODevice::WriteOnly)) {
                qCWarning(INTEGRATOR) << "Failed to open host wrapper file";
                return;
            }
            auto written = hostWrapperFile.write(data);
            if (written != data.size()) {
                qCWarning(INTEGRATOR) << "Failed to write host wrapper file";
                return;
            }
        }

        { // hosts definition
            const QString extensionDefinitionDir = m_hostWrapperDir + "/.mozilla/native-messaging-hosts"_L1;
            QDir().mkpath(extensionDefinitionDir);

            auto defintionsDirFd = openNoSymlinks(qUtf8Printable(extensionDefinitionDir), O_PATH);
            const auto closeDefintionsDirFd = qScopeGuard([defintionsDirFd]() {
                close(defintionsDirFd);
            });
            if (defintionsDirFd == -1) {
                auto err = errno;
                qCWarning(INTEGRATOR) << "Failed to open hostWrapper directory." << extensionDefinitionDir << ":" << safe_strerror(err);
                return;
            }

            auto hostDefintionFd =
                openat(defintionsDirFd, "org.kde.plasma.browser_integration.json", O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC | AT_SYMLINK_NOFOLLOW, S_IRWXU);
            auto closeHostDefintionFd = qScopeGuard([hostDefintionFd]() {
                close(hostDefintionFd);
            });
            if (hostDefintionFd == -1) {
                auto err = errno;
                qCWarning(INTEGRATOR) << "Failed to open host wrapper file org.kde.plasma.browser_integration.json:" << safe_strerror(err);
                return;
            }

            QFile extensionDefinition;
            if (!extensionDefinition.open(hostDefintionFd, QIODevice::WriteOnly)) {
                qCWarning(INTEGRATOR) << "Failed to open extension definition file";
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
    const QString m_hostWrapperName = "plasma-browser-integration-host"_L1;
    const QString m_hostWrapperPath = m_hostWrapperDir + "/"_L1 + m_hostWrapperName;
};

K_PLUGIN_FACTORY_WITH_JSON(BrowserIntegrationFlatpakIntegratorFactory,
                           "browserintegrationflatpakintegrator.json",
                           registerPlugin<BrowserIntegrationFlatpakIntegrator>();)

#include "plugin.moc"
