/*
    Copyright (C) 2017 Kai Uwe Broulik <kde@privat.broulik.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

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

function loadSettings() {
    chrome.storage.sync.get(DEFAULT_EXTENSION_SETTINGS, function (items) {
        if (chrome.runtime.lastError) {
            return cb(chrome.runtime.lastError);
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

    chrome.storage.sync.set(settings, function () {
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

            button.addEventListener("click", function () {
                tabClicked(tabbar, button);
            });

            tabbar.buttons.push(button);

            // start with the one tab page that is active
            if (tabtarget.classList.contains("active")) {
                tabClicked(tabbar, button);
            }
        });
    });

    loadSettings();

    document.getElementById("save").addEventListener("click", function () {
        var saveMessage = document.getElementById("save-message");
        saveMessage.innerText = "";

        saveSettings(function (error) {
            if (error) {
                saveMessage.innerText = "Saving settings failed";
                return;
            }

            saveMessage.innerText = "Settings successfully saved";
            (chrome.extension.sendMessage || browser.runtime.sendMessage)({
                subsystem: "settings",
                action: "changed"
            });
        });
    });

    document.getElementById("clear-settings").addEventListener("click", function () {
        chrome.storage.sync.clear(function () {
            if (chrome.runtime.lastError) {
                console.warn("Clearing settings failed", chrome.runtime.lastError);
                return;
            }

            console.log("Settings cleared");
            loadSettings();
        });
    });

});
