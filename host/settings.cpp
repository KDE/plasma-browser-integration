/*
    Copyright (C) 2017 by Kai Uwe Broulik <kde@privat.broulik.de>
    Copyright (C) 2017 by David Edmundson <davidedmundson@kde.org>

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
