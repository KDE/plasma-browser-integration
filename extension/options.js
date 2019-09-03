/*
    Copyright (C) 2017 Kai Uwe Broulik <kde@privat.broulik.de>
    Copyright (C) 2018 David Edmundson <davidedmundson@kde.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

var storage = (IS_FIREFOX ? chrome.storage.local : chrome.storage.sync);

function tabClicked(tabbar, tabbutton) {
    tabbar.buttons.forEach(function (button) {
        var tablink = button.dataset.tabLink

        var tabtarget = document.querySelector("[data-tab-id=" + tablink + "]");

        if (tabbutton == button) {
            button.classList.add("active");
            tabtarget.classList.add("active");
        } else {
            button.classList.remove("active");
            tabtarget.classList.remove("active");
        }
    });
}

function loadSettings(cb) {
    storage.get(DEFAULT_EXTENSION_SETTINGS, function (items) {
        if (chrome.runtime.lastError) {
            if (typeof cb === "function") {
                cb(false);
            }
            return;
        }

        for (let key in items) {
            if (!items.hasOwnProperty(key)) {
                continue;
            }

            let controls = document.querySelectorAll("[data-extension=" + key + "]");
            for (let control of controls) {
                let settingsKey = control.dataset.settingsKey;
                if (!settingsKey) {
                    console.warn("Invalid settings key in", control, "cannot load this");
                    continue;
                }

                let value = items[key][settingsKey]

                if (control.type === "checkbox") {
                    control.checked = !!value;
                } else {
                    if (value === true) {
                        control.value = "TRUE";
                    } else if (value === false) {
                        control.value = "FALSE";
                    } else {
                        control.value = value;
                    }
                }

                updateDependencies(control, key, settingsKey);

                control.addEventListener("change", () => {
                    let saveMessage = document.getElementById("save-message");
                    saveMessage.innerText = "";

                    updateDependencies(control, key, settingsKey);

                    saveSettings((error) => {
                        if (error) {
                            try {
                                saveMessage.innerText = chrome.i18n.getMessage("options_save_failed");
                            } catch (e) {
                                // When the extension is reloaded, any call to extension APIs throws, make sure we show at least some form of error
                                saveMessage.innerText = "Saving settings failed (" + (error || e) + ")";
                            }
                            return;
                        }

                        sendMessage("settings", "changed");
                    });
                });
            }
        }

        if (typeof cb === "function") {
            cb(true);
        }
    });
}

function saveSettings(cb) {
    var settings = {};

    let controls = document.querySelectorAll("[data-extension]");
    for (let control of controls) {
        let extension = control.dataset.extension;

        if (!DEFAULT_EXTENSION_SETTINGS.hasOwnProperty(extension)) {
            console.warn("Cannot save settings for extension", extension, "which isn't in DEFAULT_EXTENSION_SETTINGS");
            continue;
        }

        let settingsKey = control.dataset.settingsKey;
        if (!settingsKey) {
            console.warn("Invalid settings key in", control, "cannot save this");
            continue;
        }

        if (!settings[extension]) {
            settings[extension] = {};
        }

        if (!DEFAULT_EXTENSION_SETTINGS[extension].hasOwnProperty(settingsKey)) {
            console.warn("Cannot save settings key", settingsKey, "in extension", extension, "which isn't in DEFAULT_EXTENSION_SETTINGS");
            continue;
        }

        if (control.type === "checkbox") {
            settings[extension][settingsKey] = control.checked;
        } else {
            let value = control.value;
            if (value === "TRUE") {
                value = true;
            } else if (value === "FALSE") {
                value = false;
            }
            settings[extension][settingsKey] = value;
        }
    }

    try {
        storage.set(settings, function () {
            return cb(chrome.runtime.lastError);
        });
    // When the extension is reloaded, any call to extension APIs throws
    } catch (e) {
        cb(e);
    }
}

function updateDependencies(control, extension, settingsKey) {
    // Update all depending controls
    let value = control.type === "checkbox" ? control.checked : control.value;
    if (value === true) {
        value = "TRUE";
    } else if (value === false) {
        value = "FALSE";
    }

    let dependencies = document.querySelectorAll("[data-depends-extension=" + extension + "][data-depends-settings-key=" + settingsKey + "]");
    for (let dependency of dependencies) {
        dependency.disabled = (value != dependency.dataset.dependsSettingsValue);
    }
}

document.addEventListener("DOMContentLoaded", function () {

    // poor man's tab widget :)
    document.querySelectorAll(".tabbar").forEach(function (tabbar) {
        tabbar.buttons = [];

        tabbar.querySelectorAll("[data-tab-link]").forEach(function (button) {

            var tablink = button.dataset.tabLink

            var tabtarget = document.querySelector("[data-tab-id=" + tablink + "]");

            button.addEventListener("click", function (event) {
                tabClicked(tabbar, button);
                event.preventDefault();
            });

            tabbar.buttons.push(button);

            // start with the one tab page that is active
            if (tabtarget.classList.contains("active")) {
                tabClicked(tabbar, button);
            }
        });
    });

    if (IS_FIREFOX) {
        document.querySelectorAll("[data-not-show-in=firefox]").forEach(function (item) {
            item.style.display = "none";
        });
    }

    // check whether the platform is supported before loading and activating settings
    chrome.runtime.getPlatformInfo(function (info) {
        if (!SUPPORTED_PLATFORMS.includes(info.os)) {
            document.body.classList.add("os-not-supported");
            return;
        }

        loadSettings();

        // When getSubsystemStatus fails we assume it's an old host without any of the new features
        // for which we added the requires-extension attributes. Disable all of them initially
        // and then have the supported ones enabled below.
        document.querySelectorAll("[data-requires-extension]").forEach((item) => {
            item.classList.add("not-supported", "by-host");
        });

        sendMessage("settings", "getSubsystemStatus").then((status) => {
            document.querySelectorAll("[data-requires-extension]").forEach((item) => {
                let requiresExtension = item.dataset.requiresExtension;

                if (requiresExtension && !status.hasOwnProperty(requiresExtension)) {
                    console.log("Extension", requiresExtension, "is not supported by this version of the host");
                    return; // continue
                }

                let requiresMinimumVersion = Number(item.dataset.requiresExtensionVersionMinimum);
                if (requiresMinimumVersion) {
                    let runningVersion = status[requiresExtension].version;
                    if (runningVersion < requiresMinimumVersion) {
                        console.log("Extension", requiresExtension, "of version", requiresMinimumVersion, "is required but only", runningVersion, "is present in the host");
                        return; // continue
                    }
                }

                item.classList.remove("not-supported", "by-host");
            });
        }).catch((e) => {
            // The host is most likely not working correctly
            // If we run this against an older host which doesn't support message replies
            // this handler is never entered, so we really encountered an error just now!
            console.warn("Failed to determine subsystem status", e);
            document.body.classList.add("startup-failure");
        });
    });

    document.getElementById("open-krunner-settings").addEventListener("click", function (event) {
        sendMessage("settings", "openKRunnerSettings");
        event.preventDefault();
    });

    // Make translators credit behave like the one in KAboutData
    var translatorsAboutData = "";

    var translators = chrome.i18n.getMessage("options_about_translators");
    if (translators && translators !== "Your names") {
        translatorsAboutData = chrome.i18n.getMessage("options_about_translated_by", translators)
    }

    var translatorsAboutDataItem = document.getElementById("translators-aboutdata");
    if (translatorsAboutData) {
        translatorsAboutDataItem.innerText = translatorsAboutData;
    } else {
        translatorsAboutDataItem.style.display = "none";
    }
});
