/*
    Copyright (C) 2017 Kai Uwe Broulik <kde@privat.broulik.de>

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

function sendEnvironment() {
    var browser = "";

    var ua = navigator.userAgent;
    // Try to match the most derived first
    if (ua.match(/vivaldi/i)) {
        browser = "vivaldi";
    } else if(ua.match(/OPR/i)) {
        browser = "opera";
    } else if(ua.match(/chrome/i)) {
        browser = "chromium";
        // Apparently there is no better way to distinuish chromium from chrome
        for (i in window.navigator.plugins) {
            if (window.navigator.plugins[i].name === "Chrome PDF Viewer") {
                browser = "chrome";
                break;
            }
        }
    } else if(ua.match(/firefox/i)) {
        browser = "firefox";
    }

    sendPortMessage("settings", "setEnvironment", {browserName: browser});
}

function sendSettings() {
    storage.get(DEFAULT_EXTENSION_SETTINGS, function (items) {
        if (chrome.runtime.lastError) {
            console.warn("Failed to load settings");
            return;
        }

        sendPortMessage("settings", "changed", items);
    });
}

// activates giveb tab and raises its window, used by tabs runner and mpris Raise command
function raiseTab(tabId) {
// first activate the tab, this means it's current in its window
    chrome.tabs.update(tabId, {active: true}, function (tab) {

        if (chrome.runtime.lastError || !tab) { // this "lastError" stuff feels so archaic
            // failed to update
            return;
        }

        // then raise the tab's window too
        chrome.windows.update(tab.windowId, {focused: true});
    });
}

// Debug
// ------------------------------------------------------------------------
//
addCallback("debug", "debug", function(payload) {
    console.log("From host:", payload.message);
}
)

addCallback("debug", "warning", function(payload) {
    console.warn("From host:", payload.message);
}
)

// System
// ------------------------------------------------------------------------
//

// When connecting to native host fails (e.g. not installed), we immediately get a disconnect
// event immediately afterwards. Also avoid infinite restart loop then.
var receivedMessageOnce = false;

// Check for supported platform to avoid loading it on e.g. Windows and then failing
// when the extension got synced to another device and then failing
chrome.runtime.getPlatformInfo(function (info) {
    if (!SUPPORTED_PLATFORMS.includes(info.os)) {
        console.log("This extension is not supported on", info.os);
        return;
    }

    connectHost();
});

function connectHost() {
    port = chrome.runtime.connectNative("org.kde.plasma.browser_integration");

    port.onMessage.addListener(function (message) {
        var subsystem = message.subsystem;
        var action = message.action;

        let isReply = message.hasOwnProperty("replyToSerial");
        let replyToSerial = message.replyToSerial;

        if (!isReply && (!subsystem || !action)) {
            return;
        }

        receivedMessageOnce = true;

        if (isReply) {
            let replyResolver = pendingMessageReplyResolvers[replyToSerial];
            if (replyResolver) {
                replyResolver(message.payload);
                delete pendingMessageReplyResolvers[replyToSerial];
            } else {
                console.warn("There is no reply resolver for message with serial", replyToSerial);
            }
            return;
        }

        if (callbacks[subsystem] && callbacks[subsystem][action]) {
            callbacks[subsystem][action](message.payload, action);
        } else {
            console.warn("Don't know what to do with host message", subsystem, action);
        }
    });

    port.onDisconnect.addListener(function() {
        var error = chrome.runtime.lastError;

        console.warn("Host disconnected", error);

        // Remove all kde connect menu entries since they won't work without a host
        for (let device of kdeConnectDevices) {
            chrome.contextMenus.remove(kdeConnectMenuIdPrefix + device);
        }
        kdeConnectDevices = [];

        var reason = chrome.i18n.getMessage("general_error_unknown");
        if (error && error.message) {
            reason = error.message;
        }

        var message = receivedMessageOnce ? chrome.i18n.getMessage("general_error_port_disconnect", reason)
                                          : chrome.i18n.getMessage("general_error_port_startupfail");

        chrome.notifications.create(null, {
            type: "basic",
            title: chrome.i18n.getMessage("general_error_title"),
            message: message,
            iconUrl: "icons/sad-face-128.png"
        });

        if (receivedMessageOnce) {
            console.log("Auto-restarting it");
            connectHost();
        } else {
            console.warn("Not auto-restarting host as we haven't received any message from it before. Check that it's working/installed correctly");
        }
    });

    sendEnvironment();
    sendSettings();
    sendDownloads();
}

addRuntimeCallback("settings", "changed", function () {
    // we could also just reload our extension :)
    // but this also causes the settings dialog to quit
    //chrome.runtime.reload();
    sendSettings();
});

addRuntimeCallback("settings", "openKRunnerSettings", function () {
    sendPortMessage("settings", "openKRunnerSettings");
});
