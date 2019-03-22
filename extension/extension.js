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

var port
var callbacks = {}; // TODO rename to "portCallbacks"?
var runtimeCallbacks = {};

// tracks whether an extension is loaded and what version
var subsystemStatus = {};

function addCallback(subsystem, action, callback) // TODO rename to "addPortCallbacks"?
{
    if (action.constructor === Array) {
        action.forEach(function(item) {
            addCallback(subsystem, item, callback);
        });
        return;
    }

    if (!callbacks[subsystem]) {
        callbacks[subsystem] = {};
    }
    callbacks[subsystem][action] = callback;
}

function sendPortMessage(subsystem, event, payload)
{
    // why do we put stuff on root level here but otherwise have a "payload"? :(
    var message = payload || {}
    message.subsystem = subsystem;
    message.event = event;

    port.postMessage(message);
}

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
    var storage = (IS_FIREFOX ? chrome.storage.local : chrome.storage.sync);

    storage.get(DEFAULT_EXTENSION_SETTINGS, function (items) {
        if (chrome.runtime.lastError) {
            console.warn("Failed to load settings");
            return;
        }

        sendPortMessage("settings", "changed", items);
    });
}

function addRuntimeCallback(subsystem, action, callback)
{
    if (action.constructor === Array) {
        action.forEach(function(item) {
            addRuntimeCallback(subsystem, item, callback);
        });
        return;
    }

    if (!runtimeCallbacks[subsystem]) {
        runtimeCallbacks[subsystem] = {};
    }
    runtimeCallbacks[subsystem][action] = callback;
}

// returns an Object which only contains values for keys in allowedKeys
function filterObject(obj, allowedKeys) {
    var newObj = {}

    // I bet this can be done in a more efficient way
    for (key in obj) {
        if (obj.hasOwnProperty(key) && allowedKeys.indexOf(key) > -1) {
            newObj[key] = obj[key];
        }
    }

    return newObj;
}

// filters objects within an array so they only contain values for keys in allowedKeys
function filterArrayObjects(arr, allowedKeys) {
    return arr.map(function (item) {
        return filterObject(item, allowedKeys);
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

// KDE Connect
// ------------------------------------------------------------------------
//

var kdeConnectMenuIdPrefix = "kdeconnect_page_";

chrome.contextMenus.onClicked.addListener(function (info) {
    if (!info.menuItemId.startsWith(kdeConnectMenuIdPrefix)) {
        return;
    }

    var deviceId = info.menuItemId.substr(kdeConnectMenuIdPrefix.length);

    var url = info.linkUrl || info.pageUrl;
    console.log("Send url", url, "to kdeconnect device", deviceId);
    if (!url) {
        return;
    }

    port.postMessage({
        subsystem: "kdeconnect",
        event: "shareUrl",
        url: url,
        deviceId: deviceId
    });
});

addCallback("kdeconnect", "deviceAdded", function(message) {
    var id = message.id;
    var name = message.name;

    var menuEntryTitle = chrome.i18n.getMessage("kdeconnect_open_device", name);
    var menuId = kdeConnectMenuIdPrefix + id;

    chrome.contextMenus.create({
        id: menuId,
        contexts: ["link", "page"],
        title: menuEntryTitle,
    });
});

addCallback("kdeconnect", "deviceRemoved", function(message) {
    chrome.contextMenus.remove("kdeconnect_page_" + message.id)
});

// MPRIS
// ------------------------------------------------------------------------
//

var playerTabIds = [];

function currentPlayerTabId() {
    return playerTabIds[playerTabIds.length - 1] || 0;
}

function playerGone(tabId) {
    var oldPlayer = currentPlayerTabId();

    var removedPlayerIdx = playerTabIds.indexOf(tabId);
    if (removedPlayerIdx > -1) {
        playerTabIds.splice(removedPlayerIdx, 1); // remove that player from the array
    }

    var newPlayer = currentPlayerTabId();

    if (oldPlayer == newPlayer) {
        return;
    }

    // all players gone :(
    if (!newPlayer) {
        sendPortMessage("mpris", "gone");
        return;
    }

    // ask the now current player to identify to us
    // we can't just pretend "playing" as the other player might be paused
    chrome.tabs.sendMessage(newPlayer, {
        subsystem: "mpris",
        action: "identify"
    });
}

// when tab is closed, tell the player is gone
// below we also have a "gone" signal listener from the content script
// which is invoked in the onbeforeunload handler of the page
chrome.tabs.onRemoved.addListener(playerGone);

// callbacks from host (Plasma) to our extension
addCallback("mpris", "raise", function (message) {
    var playerTabId = currentPlayerTabId();
    if (playerTabId) {
        raiseTab(playerTabId);
    }
});

addCallback("mpris", ["play", "pause", "playPause", "stop", "next", "previous"], function (message, action) {
    var playerTabId = currentPlayerTabId();
    if (playerTabId)  {
        chrome.tabs.sendMessage(playerTabId, {
            subsystem: "mpris",
            action: action
        });
    }
});

addCallback("mpris", "setVolume", function (message) {
    var playerTabId = currentPlayerTabId();
    if (playerTabId)  {
        chrome.tabs.sendMessage(playerTabId, {
            subsystem: "mpris",
            action: "setVolume",
            payload: {
                volume: message.volume
            }
        });
    }
});

addCallback("mpris", "setLoop", function (message) {
    var playerTabId = currentPlayerTabId();
    if (playerTabId)  {
        chrome.tabs.sendMessage(playerTabId, {
            subsystem: "mpris",
            action: "setLoop",
            payload: {
                loop: message.loop
            }
        });
    }
});

addCallback("mpris", "setPosition", function (message) {
    var playerTabId = currentPlayerTabId();
    if (playerTabId)  {
        chrome.tabs.sendMessage(playerTabId, {
            subsystem: "mpris",
            action: "setPosition",
            payload: {
                position: message.position
            }
        });
    }
})

addCallback("mpris", "setPlaybackRate", function (message) {
    var playerTabId = currentPlayerTabId();
    if (playerTabId)  {
        chrome.tabs.sendMessage(playerTabId, {
            subsystem: "mpris",
            action: "setPlaybackRate",
            payload: {
                playbackRate: message.playbackRate
            }
        });
    }
});

// callbacks from a browser tab to our extension
addRuntimeCallback("mpris", "playing", function (message, sender) {
    // Chrome doesn't run extensions in incognito by default but Firefox does
    // so we disable media controls for them to prevent accidental private
    // information leak on lock screen or now playing auto status in a messenger
    if (IS_FIREFOX && sender.tab.incognito) {
        return;
    }

    var idx = playerTabIds.indexOf(sender.tab.id);
    if (idx > -1) {
        // Move it to the end of the list so it becomes current
        playerTabIds.push(playerTabIds.splice(idx, 1)[0]);
    } else {
        playerTabIds.push(sender.tab.id);
    }

    var payload = message || {};
    payload.tabTitle = sender.tab.title;
    payload.url = sender.tab.url;

    sendPortMessage("mpris", "playing", payload);
});

addRuntimeCallback("mpris", "gone", function (message, sender) {
    playerGone(sender.tab.id);
});

addRuntimeCallback("mpris", "stopped", function (message, sender) {
    // When player stopped, check if there's another one we could control now instead
    if (currentPlayerTabId() == sender.tab.id) {
        if (playerTabIds.length > 1) {
            playerGone(sender.tab.id);
        }
    }
});

addRuntimeCallback("mpris", ["paused", "waiting", "canplay"], function (message, sender, action) {
    if (currentPlayerTabId() == sender.tab.id) {
        sendPortMessage("mpris", action);
    }
});

addRuntimeCallback("mpris", ["duration", "timeupdate", "seeking", "seeked", "ratechange", "volumechange", "titlechange"], function (message, sender, action) {
    if (currentPlayerTabId() == sender.tab.id) {
        sendPortMessage("mpris", action, message);
    }
});

addRuntimeCallback("mpris", ["metadata", "callbacks"], function (message, sender, action) {
    if (currentPlayerTabId() == sender.tab.id) {
        var payload = {};
        payload[action] = message;

        sendPortMessage("mpris", action, payload);
    }
});

// MISC
// ------------------------------------------------------------------------
//



// Downloads
// ------------------------------------------------------------------------
//

var activeDownloads = []
var downloadUpdateInterval = 0;

function startSendingDownloadUpdates() {
    if (!downloadUpdateInterval) {
        downloadUpdateInterval = setInterval(sendDownloadUpdates, 1000);
    }
}

function stopSendingDownloadUpdates() {
    if (downloadUpdateInterval) {
        clearInterval(downloadUpdateInterval);
        downloadUpdateInterval = 0;
    }
}

function sendDownloadUpdates() {
    chrome.downloads.search({
        state: 'in_progress',
        paused: false
    }, function (results) {
        if (!results.length) {
            stopSendingDownloadUpdates();
            return;
        }

        results.forEach(function (download) {
            if (activeDownloads.indexOf(download.id) === -1) {
                return;
            }

            var payload = {
                id: download.id,
                bytesReceived: download.bytesReceived,
                estimatedEndTime: download.estimatedEndTime,
                // Firefox ends along "-1" as totalBytes on download creation
                // but then never updates it, so we send this along periodically, too
                totalBytes: download.totalBytes
            };

            port.postMessage({subsystem: "downloads", event: "update", download: payload});
        });
    });
}

// only forward certain download properties back to our host
var whitelistedDownloadProperties = [
    "id", "url", "finalUrl", "filename", "startTime", "estimatedEndTime", "totalBytes", "bytesReceived", "state", "error", /*"canResume"*/, "paused"
];

function createDownload(download) {
    // don't bother telling us about completed downloads...
    // otherwise on browser startup we'll spawn a gazillion download progress notification
    if (download.state === "complete" || download.state === "interrupted") {
        return;
    }

    var filteredDownload = filterObject(download, whitelistedDownloadProperties);

    activeDownloads.push(download.id);
    startSendingDownloadUpdates();

    port.postMessage({subsystem: "downloads", event: "created", download: filteredDownload});
}

function sendDownloads() {
    // When extension is (re)loaded, create each download initially
    chrome.downloads.search({
        state: 'in_progress',
    }, function (results) {
        results.forEach(createDownload);
    });
}

chrome.downloads.onCreated.addListener(createDownload);

chrome.downloads.onChanged.addListener(function (delta) {
    if (activeDownloads.indexOf(delta.id) === -1) {
        return;
    }

    // An interrupted download was resumed. When a download is interrupted, we finish (and delete)
    // the job but the browser re-uses the existing download, so when this happen,
    // pretend a new download was created.
    if (delta.state) {
        if (delta.state.previous === "interrupted" && delta.state.current === "in_progress") {
            console.log("Resuming previously interrupted download, pretending a new download was created");
            chrome.downloads.search({
                id: delta.id
            }, function (downloads) {
                createDownload(downloads[0]);
            });
            return;
        }
    }

    var payload = {};

    whitelistedDownloadProperties.forEach(function (item) {
        if (delta[item]) {
            payload[item] = delta[item].current;
        }
    });

    payload.id = delta.id; // not a delta, ie. has no current and thus isn't added by the loop below

    port.postMessage({subsystem: "downloads", event: "update", download: payload});
});

addCallback("downloads", "cancel", function (message) {
    var downloadId = message.downloadId;

    console.log("Requested to cancel download", downloadId);

    chrome.downloads.cancel(downloadId);
});

addCallback("downloads", "suspend", function (message) {
    var downloadId = message.downloadId;

    console.log("Requested to suspend download", downloadId);

    chrome.downloads.pause(downloadId);
});

addCallback("downloads", "resume", function (message) {
    var downloadId = message.downloadId;

    console.log("Requested to resume download", downloadId);

    chrome.downloads.resume(downloadId);
});

// Tabs Runner
// ------------------------------------------------------------------------
//
addCallback("tabsrunner", "activate", function (message) {
    var tabId = message.tabId;

    console.log("Tabs Runner requested to activate tab with id", tabId);

    raiseTab(tabId);
});

addCallback("tabsrunner", "setMuted", function (message) {

    var tabId = message.tabId;
    var muted = message.muted;

    chrome.tabs.update(tabId, {muted: muted}, function (tab) {

        if (chrome.runtime.lastError || !tab) { // this "lastError" stuff feels so archaic
            // failed to mute/unmute
            return;
        }
    });

});

// only forward certain tab properties back to our host
var whitelistedTabProperties = [
    "id", "active", "audible", "favIconUrl", "incognito", "title", "url", "mutedInfo"
];

// FIXME We really should enforce some kind of security policy, so only e.g. plasmashell and krunner
// may access your tabs
addCallback("tabsrunner", "getTabs", function (message) {
    chrome.tabs.query({}, function (tabs) {
        // remove incognito tabs and properties not in whitelist
        var filteredTabs = tabs;

        // Chrome doesn't run extensions in incognito by default but Firefox does so we exclude those tabs for it
        if (IS_FIREFOX) {
            filteredTabs = filteredTabs.filter(function (tab) {
                return !tab.incognito;
            });
        }

        var filteredTabs = filterArrayObjects(filteredTabs, whitelistedTabProperties);

        // Shared between the callbacks
        var total = filteredTabs.length;

        var sendTabsIfComplete = function() {
            if (--total > 0) {
                return;
            }

            port.postMessage({
                subsystem: "tabsrunner",
                event: "gotTabs",
                tabs: filteredTabs
            });
        };

        for (let tabIndex in filteredTabs) {
            let currentIndex = tabIndex; // Not shared
            var favIconUrl = filteredTabs[tabIndex].favIconUrl;

            if (!favIconUrl) {
                sendTabsIfComplete();
            } else if (favIconUrl.match(/^data:image/)) {
                // Already a data URL
                filteredTabs[currentIndex].favIconData = favIconUrl;
                filteredTabs[currentIndex].favIconUrl = "";
                sendTabsIfComplete();
            } else {
                // Send a request to fill the cache (=no timeout)
                let xhrForCache = new XMLHttpRequest();
                xhrForCache.open("GET", favIconUrl);
                xhrForCache.send();

                // Try to fetch from (hopefully) the cache (100ms timeout)
                let xhr = new XMLHttpRequest();
                xhr.onreadystatechange = function() {
                    if (xhr.readyState != 4) {
                        return;
                    }

                    if (!xhr.response) {
                        filteredTabs[currentIndex].favIconData = "";
                        sendTabsIfComplete();
                        return;
                    }

                    var reader = new FileReader();
                    reader.onloadend = function() {
                        filteredTabs[currentIndex].favIconData = reader.result;
                        sendTabsIfComplete();
                    }
                    reader.readAsDataURL(xhr.response);
                };
                xhr.open('GET', favIconUrl);
                xhr.responseType = 'blob';
                xhr.timeout = 100;
                xhr.send();
            }
        }
    });
});

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

        if (!subsystem || !action) {
            return;
        }

        receivedMessageOnce = true;

        // keeps track of what extensions are loaded and in what version in subsystemStatus
        if (action === "created") {
            subsystemStatus[subsystem] = {
                version: message.payload.version,
                loaded: false
            };
            return;
        } else if (action === "loaded") {
            subsystemStatus[subsystem].loaded = true;
            return;
        } else if (action === "unloaded") {
            subsystemStatus[subsystem].loaded = false;
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

chrome.runtime.onMessage.addListener(function (message, sender, sendResponse) {
    // TODO check sender for privilege

    var subsystem = message.subsystem;
    var action = message.action;

    if (!subsystem || !action) {
        return;
    }

    if (runtimeCallbacks[subsystem] && runtimeCallbacks[subsystem][action]) {
        runtimeCallbacks[subsystem][action](message.payload, sender, action);
    } else {
        console.warn("Don't know what to do with runtime message", subsystem, action);
    }
});
