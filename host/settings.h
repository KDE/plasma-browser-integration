#pragma once

#include "abstractbrowserplugin.h"

/*
 * This class manages the extension's settings (so that settings in the browser
 * propagate to our extension) and also detects the environment the host is run
 * in (e.g. whether we're started by Firefox, Chrome, Chromium, or Opera)
 */
class Settings : public AbstractBrowserPlugin
{
    Q_OBJECT

public:
    static Settings &self();

    enum class Environment {
        Unknown,
        Chrome,
        Chromium, // can we actually distinguish Chromium from Chrome?
        Firefox,
        Opera
    };
    Q_ENUM(Environment)

    void handleData(const QString &event, const QJsonObject &data);

    Environment environment() const;
    // TODO should we have additional getters like browserName(), browserDesktopEntry(), etc?

private:
    Settings();
    ~Settings() override = default;

    Environment m_environment = Environment::Unknown;

};
