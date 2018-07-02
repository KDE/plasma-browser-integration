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

function sendMessage(action, payload)
{
    (chrome.extension.sendMessage || browser.runtime.sendMessage)({
        subsystem: "settings",
        action: action,
        payload: payload
    });
}

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

function extensionCheckboxes() {
    return document.querySelectorAll("#extensions-selection input[type=checkbox][data-extension]");
}

function loadSettings(cb) {
    storage.get(DEFAULT_EXTENSION_SETTINGS, function (items) {
        if (chrome.runtime.lastError) {
            if (typeof cb === "function") {
                cb(false);
            }
            return;
        }

        for (var key in items) {
            if (!items.hasOwnProperty(key)) {
                continue;
            }

            var checkbox = document.querySelector("input[type=checkbox][data-extension=" + key + "]");
            if (!checkbox) {
                console.warn("Failed to find checkbox for extension", key);
                continue;
            }

            var checked = !!items[key].enabled;
            checkbox.checked = checked;

            // TODO restore additional stuff if we have it
        }

        if (typeof cb === "function") {
            cb(true);
        }
    });
}

function saveSettings(cb) {
    var settings = {};

    for (var key in DEFAULT_EXTENSION_SETTINGS) {
        if (!DEFAULT_EXTENSION_SETTINGS.hasOwnProperty(key)) {
            continue;
        }

        var checkbox = document.querySelector("input[type=checkbox][data-extension=" + key + "]");
        if (!checkbox) {
            console.warn("Failed to find checkbox for extension", key);
            continue;
        }

        settings[key] = {
            enabled: checkbox.checked
        };

        // TODO save additional stuff if we have it
    }

    storage.set(settings, function () {
        return cb(chrome.runtime.lastError);
    });
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
            document.body.classList.add("not-supported");
            return;
        }

        loadSettings(function () {
            var mpris = document.querySelector("[data-extension=mpris]");
            var mprisEx = document.querySelector("[data-extension=mprisMediaSessions]");
            mpris.addEventListener("change", function() {
                mprisEx.disabled = !mpris.checked;
            });
            mprisEx.disabled = !mpris.checked;
        });

        // auto save when changing any setting
        // TODO can we do that on closing, or does it not matter how often we do chrome storage sync thing?
        document.querySelectorAll("input[type=checkbox]").forEach(function (item) {
            item.addEventListener("click", function () {
                var saveMessage = document.getElementById("save-message");
                saveMessage.innerText = "";

                saveSettings(function (error) {
                    if (error) {
                        saveMessage.innerText = chrome.i18n.getMessage("options_save_failed");
                        return;
                    }

                    sendMessage("changed");
                });
            });
        });
    });

    document.getElementById("open-krunner-settings").addEventListener("click", function (event) {
        sendMessage("openKRunnerSettings");
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
