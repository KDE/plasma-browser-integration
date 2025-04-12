// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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
    /** Enum for different browser types, i.e. which browser they are based on. */
    enum BrowserBase {
        Firefox,
        Chrome,
        Chromium,
    };
    Q_ENUM(BrowserBase)

    /** Browser information structure to hold browser-specific configuration. */
    struct BrowserInfo {
        BrowserBase base;

        /** The browser's Flatpak id, e.g. "org.mozilla.firefox". */
        QString id;

        /** The directory the browser expects its native messaging hosts to be in. */
        QString nativeMessagingHostsDir;
    };

    BrowserIntegrationFlatpakIntegrator(QObject *parent, const QList<QVariant> &)
        : KDEDModule(parent)
    {
        // List of Flatpak browsers we support.
        const QList<BrowserInfo> supportedBrowsers = {
            {BrowserBase::Firefox, u"org.mozilla.firefox"_s, u"/.mozilla/native-messaging-hosts"_s},
            {BrowserBase::Firefox, u"io.gitlab.librewolf-community"_s, u"/.librewolf/native-messaging-hosts"_s},
            {BrowserBase::Chrome, u"com.google.Chrome"_s, u"/config/google-chrome/NativeMessagingHosts"_s},
            {BrowserBase::Chrome, u"com.google.ChromeDev"_s, u"/config/google-chrome-unstable/NativeMessagingHosts"_s},
            {BrowserBase::Chromium, u"org.chromium.Chromium"_s, u"/config/chromium/NativeMessagingHosts"_s},
            {BrowserBase::Chromium, u"io.github.ungoogled_software.ungoogled_chromium"_s, u"/config/chromium/NativeMessagingHosts"_s},
        };

        // Set up Flatpak permissions for each browser
        for (const auto &browser : supportedBrowsers) {
            auto flatpak = new QProcess(this);
            connect(flatpak, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [flatpak](int, QProcess::ExitStatus) {
                flatpak->deleteLater();
            });
            flatpak->start(u"flatpak"_s, {u"override"_s, u"--user"_s, u"--talk-name=org.kde.plasma.browser.integration"_s, browser.id});
        }

        // Register on separate bus connection to avoid exposing other services to the sandbox
        auto connection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "org.kde.plasma.browser.integration"_L1);
        connection.registerService("org.kde.plasma.browser.integration"_L1);
        connection.registerObject("/org/kde/plasma/browser/integration"_L1, this, QDBusConnection::ExportAllSlots);

        // Create messaging hosts for each supported browser
        for (const auto &browser : supportedBrowsers) {
            createMessagingHost(browser); // always create integration regardless of the browser being installed so we can be ready for if it is installed
        }
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
#ifdef STRERROR_R_CHAR_P
        return strerror_r(error, buffer.data(), buffer.size());
#else
        // Won't be changed by strerror_r but not const so compiler doesn't throw an error
        static char unknown[] = "unknown error";

        return strerror_r(error, buffer.data(), buffer.size()) ? unknown : buffer.data();
#endif
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

    void createMessagingHost(const BrowserInfo &browser)
    {
        const QString hostWrapperDir = QDir::homePath() + QStringLiteral("/.var/app/") + browser.id;
        const QString hostWrapperName = "plasma-browser-integration-host"_L1;
        const QString hostWrapperPath = hostWrapperDir + "/"_L1 + hostWrapperName;

        QDir().mkpath(hostWrapperDir);

        auto hostWrapperDirFd = openNoSymlinks(qUtf8Printable(hostWrapperDir), O_PATH);
        const auto closeHostWrapperDirFd = qScopeGuard([hostWrapperDirFd]() {
            close(hostWrapperDirFd);
        });
        if (hostWrapperDirFd == -1) {
            auto err = errno;
            qCWarning(INTEGRATOR) << "Failed to open hostWrapper directory." << hostWrapperDir << ":" << safe_strerror(err);
            return;
        }

        { // host wrapper
            auto hostWrapperFd = openat(hostWrapperDirFd, qUtf8Printable(hostWrapperName), O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC | O_NOFOLLOW, S_IRWXU);
            auto closeHostWrapperFd = qScopeGuard([hostWrapperFd]() {
                close(hostWrapperFd);
            });
            if (hostWrapperFd == -1) {
                auto err = errno;
                qCWarning(INTEGRATOR) << "Failed to open host wrapper file" << hostWrapperName << ":" << safe_strerror(err);
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
            const QString extensionDefinitionDir = hostWrapperDir + browser.nativeMessagingHostsDir;
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
                {u"path"_s, hostWrapperPath},
                {u"type"_s, u"stdio"_s},
            });

            // Add browser-specific fields
            if (browser.base == BrowserBase::Firefox) {
                extensionDefinitionObject.insert(u"allowed_extensions"_s, QJsonArray({u"plasma-browser-integration@kde.org"_s}));
            } else if (browser.base == BrowserBase::Chrome || browser.base == BrowserBase::Chromium) {
                extensionDefinitionObject.insert(u"allowed_origins"_s,
                                                 QJsonArray({
                                                     u"chrome-extension://cimiefiiaegbelhefglklhhakcgmhkai/"_s,
                                                     u"chrome-extension://dnnckbejblnejeabhcmhklcaljjpdjeh/"_s,
                                                 }));
            }

            extensionDefinition.write(QJsonDocument(extensionDefinitionObject).toJson());
        }
    }
};

K_PLUGIN_FACTORY_WITH_JSON(BrowserIntegrationFlatpakIntegratorFactory,
                           "browserintegrationflatpakintegrator.json",
                           registerPlugin<BrowserIntegrationFlatpakIntegrator>();)

#include "plugin.moc"
