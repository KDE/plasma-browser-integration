#include "settings.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QProcess>

#include "settingsadaptor.h"

Settings::Settings()
    : AbstractBrowserPlugin(QStringLiteral("settings"), 1, nullptr)
{
    const auto &args = QCoreApplication::arguments();
    if (args.count() > 1) {
        const QString &extensionPath = args.at(1);

        // can we actually distinguish Chromium from Chrome?
        if (extensionPath.startsWith(QLatin1String("chrome-extension:/"))) {
            m_environment = Environment::Chrome;
        } else if (extensionPath.contains(QLatin1String("mozilla"))) {
            m_environment = Environment::Firefox;
        }
        // TODO rest

        if (m_environment == Environment::Unknown) {
            qWarning() << "Failed to determin environment from extension path" << extensionPath;
        }

        qDebug() << "Extension running in" << m_environment;
    }

    new SettingsAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Settings"), this);

}

Settings &Settings::self()
{
    static Settings s_self;
    return s_self;
}

void Settings::handleData(const QString &event, const QJsonObject &data)
{
    if (event == QLatin1String("changed")) {
        m_settings = data;
        emit changed(data);
    } else if (event == QLatin1String("openKRunnerSettings")) {
        QProcess::startDetached(QStringLiteral("kcmshell5"), {QStringLiteral("kcm_plasmasearch")});
    }
}

Settings::Environment Settings::environment() const
{
    return m_environment;
}

QString Settings::environmentString() const
{
    switch (m_environment) {
    case Settings::Environment::Unknown: return QString();
    case Settings::Environment::Chrome: return QStringLiteral("chrome");
    case Settings::Environment::Chromium: return QStringLiteral("chromium");
    case Settings::Environment::Firefox: return QStringLiteral("firefox");
    case Settings::Environment::Opera: return QStringLiteral("opera");
    }

    return QString();
}

bool Settings::pluginEnabled(const QString &subsystem) const
{
    return settingsForPlugin(subsystem).value(QStringLiteral("enabled")).toBool();
}

QJsonObject Settings::settingsForPlugin(const QString &subsystem) const
{
    return m_settings.value(subsystem).toObject();
}
