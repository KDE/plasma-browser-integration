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

const QMap<Settings::Environment, QString> Settings::environmentNames = {
    {Settings::Environment::Chrome, QStringLiteral("chrome")},
    {Settings::Environment::Chromium, QStringLiteral("chromium")},
    {Settings::Environment::Firefox, QStringLiteral("firefox")},
    {Settings::Environment::Opera, QStringLiteral("opera")},
    {Settings::Environment::Vivaldi, QStringLiteral("vivaldi")},
};

Settings::Settings()
    : AbstractBrowserPlugin(QStringLiteral("settings"), 1, nullptr)
{
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
    } else if (event == QLatin1String("setEnvironment")) {
        QString name = data[QStringLiteral("browserName")].toString();
        m_environment = Settings::environmentNames.key(name, Settings::Environment::Unknown);
    }
}

Settings::Environment Settings::environment() const
{
    return m_environment;
}

QString Settings::environmentString() const
{
    return Settings::environmentNames.value(m_environment);
}

bool Settings::pluginEnabled(const QString &subsystem) const
{
    return settingsForPlugin(subsystem).value(QStringLiteral("enabled")).toBool();
}

QJsonObject Settings::settingsForPlugin(const QString &subsystem) const
{
    return m_settings.value(subsystem).toObject();
}
