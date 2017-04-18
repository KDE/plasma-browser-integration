/*
    Copyright (C) 2017 Kai Uwe Broulik <kde@privat.broulik.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

var port
var callbacks = {}; // TODO rename to "portCallbacks"?
var runtimeCallbacks = {};

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

function sendSettings() {
    chrome.storage.sync.get(DEFAULT_EXTENSION_SETTINGS, function (items) {
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

function connectHost() {
    port = chrome.runtime.connectNative("org.kde.plasma.browser_integration");

    sendSettings();
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
        chrome.windows.update(tab.windowId, {focused: true}, function (window) {

        });

        // now check if the window was minimized and then unminimize it
        // problem is that we cannot restore, we can just set state (normal or maximized)
    });
}

// KDE Connect
// ------------------------------------------------------------------------
//

var kdeConnectDefaultDeviceId = "";
var kdeConnectDefaultDeviceName = "";
var hasKdeConnectMenu = false;

chrome.contextMenus.onClicked.addListener(function (info) {
    if (info.menuItemId === "kdeconnect_page") {
        var url = info.linkUrl || info.pageUrl;
        console.log("Send url", url, "to kdeconnect device", kdeConnectDefaultDeviceId);
        port.postMessage({
            subsystem: "kdeconnect",
            event: "shareUrl",
            url: url,
            deviceId: kdeConnectDefaultDeviceId
        });
    }
});

addCallback("kdeconnect", "devicesChanged", function(message) {
    kdeConnectDefaultDeviceId = message ? message.defaultDeviceId : ""
    kdeConnectDefaultDeviceName = message ? message.defaultDeviceName : ""

    var menuEntryTitle = "Open via KDE Connect"

    if (kdeConnectDefaultDeviceName) {
        menuEntryTitle = "Open on '" + kdeConnectDefaultDeviceName + "'"
    }

    if (kdeConnectDefaultDeviceId) {
        if (hasKdeConnectMenu) {
            chrome.contextMenus.update("kdeconnect_page", {title: menuEntryTitle});
        } else {
            hasKdeConnectMenu = true; // TODO check error
            chrome.contextMenus.create({
                id: "kdeconnect_page",
                contexts: ["link", "page"],
                title: menuEntryTitle
            });
        }
    } else if (hasKdeConnectMenu) {
        chrome.contextMenus.remove("kdeconnect_page")
        hasKdeConnectMenu = false;
    }
});


// MPRIS
// ------------------------------------------------------------------------
//

var currentPlayerTabId = 0;

// when tab is closed, tell the player is gone
// below we also have a "gone" signal listener from the content script
// which is invoked in the onbeforeunload handler of the page
chrome.tabs.onRemoved.addListener(function (tabId) {
    if (tabId == currentPlayerTabId) {
        // our player is gone :(
        currentPlayerTabId = 0;
        sendPortMessage("mpris", "gone");
    }
});

// callbacks from host (Plasma) to our extension
addCallback("mpris", "raise", function (message) {
    if (currentPlayerTabId) {
        raiseTab(currentPlayerTabId);
    }
});

addCallback("mpris", ["play", "pause", "playPause", "stop", "next", "previous"], function (message, action) {
    if (currentPlayerTabId) {
        chrome.tabs.sendMessage(currentPlayerTabId, {
            subsystem: "mpris",
            action: action
        });
    }
});

addCallback("mpris", "setVolume", function (message) {
    if (currentPlayerTabId) {
        chrome.tabs.sendMessage(currentPlayerTabId, {
            subsystem: "mpris",
            action: "setVolume",
            payload: {
                volume: message.volume
            }
        });
    }
});

addCallback("mpris", "setLoop", function (message) {
    if (currentPlayerTabId) {
        chrome.tabs.sendMessage(currentPlayerTabId, {
            subsystem: "mpris",
            action: "setLoop",
            payload: {
                loop: message.loop
            }
        });
    }
});

addCallback("mpris", "setPosition", function (message) {
    if (currentPlayerTabId) {
        chrome.tabs.sendMessage(currentPlayerTabId, {
            subsystem: "mpris",
            action: "setPosition",
            payload: {
                position: message.position
            }
        });
    }
})

// callbacks from a browser tab to our extension
addRuntimeCallback("mpris", "playing", function (message, sender) {
    currentPlayerTabId = sender.tab.id;
    console.log("player tab is now", currentPlayerTabId);

    var payload = message || {};
    payload.tabTitle = sender.tab.title;
    payload.url = sender.tab.url;

    sendPortMessage("mpris", "playing", payload);
});

addRuntimeCallback("mpris", "gone", function (message, sender) {
    if (currentPlayerTabId == sender.tab.id) {
        console.log("Player navigated away");
        currentPlayerTabId = 0;
        sendPortMessage("mpris", "gone");
    }
});

addRuntimeCallback("mpris", ["paused", "stopped", "waiting", "canplay"], function (message, sender, action) {
    if (currentPlayerTabId == sender.tab.id) {
        sendPortMessage("mpris", action);
    }
});

addRuntimeCallback("mpris", ["duration", "timeupdate", "seeked", "volumechange"], function (message, sender, action) {
    if (currentPlayerTabId == sender.tab.id) {
        sendPortMessage("mpris", action, message);
    }
});

addRuntimeCallback("mpris", ["metadata", "callbacks"], function (message, sender, action) {
    if (currentPlayerTabId == sender.tab.id) {
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

setInterval(function() {
    chrome.downloads.search({
        state: 'in_progress',
        paused: false
    }, function (results) {
        if (!results.length) {
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
}, 1000);

//chrome.downloads.setShelfEnabled(false);

// only forward certain download properties back to our host
var whitelistedDownloadProperties = [
    "id", "url", "finalUrl", "filename", "startTime", "estimatedEndTime", "totalBytes", "bytesReceived", "state", "error", /*"canResume"*/, "paused"
];

chrome.downloads.onCreated.addListener(function (download) {
    // don't bother telling us about completed downloads...
    // otherwise on browser startup we'll spawn a gazillion download progress notification
    if (download.state === "complete" || download.state === "interrupted") {
        return;
    }

    var filteredDownload = filterObject(download, whitelistedDownloadProperties);

    activeDownloads.push(download.id);

    port.postMessage({subsystem: "downloads", event: "created", download: filteredDownload});
});

chrome.downloads.onChanged.addListener(function (delta) {
    if (activeDownloads.indexOf(delta.id) === -1) {
        console.log("ignoring download", delta.id, "that we didn't track");
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

// Incognito
// ------------------------------------------------------------------------
//

var incognitoTabs = [];

function tabAdded(tab) {
        console.log("got a new tab", tab);

    if (tab.incognito) {
        console.log("it's incognito");

        if (incognitoTabs.length === 0) {
            port.postMessage({subsystem: "incognito", event: "show" });
        }

        incognitoTabs.push(tab.id)
    }
}

addCallback("incognito", "close", function() {
    console.log("close all incognito tabs!");
    chrome.tabs.remove(incognitoTabs)
});


// query all tabs
chrome.tabs.query({}, function (tabs) {
    tabs.forEach(tabAdded);
});

chrome.tabs.onCreated.addListener(tabAdded);

chrome.tabs.onRemoved.addListener(function (tabId, removeInfo) {
   console.log("removed a tab", tabId);

   var idx = incognitoTabs.indexOf(tabId);
   if (idx > -1) {
       console.log("it's an incognito we know");

       incognitoTabs.splice(idx, 1); // remove item at idx

       if (incognitoTabs.length === 0) {
           console.log("no more incognito");
           port.postMessage({subsystem: "incognito", event: "hide" });
       } else {
           console.log("still know", incognitoTabs.length, "incognito tabs");
        }
   }
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
    "id", "active", "audible", "favIconUrl", "incognito", "title", "url"
];

// FIXME We really should enforce some kind of security policy, so only e.g. plasmashell and krunner
// may access your tabs
addCallback("tabsrunner", "getTabs", function (message) {
    chrome.tabs.query({}, function (tabs) {
        // remove properties not in whitelist
        var filteredTabs = filterArrayObjects(tabs, whitelistedTabProperties);

        port.postMessage({
            subsystem: "tabsrunner",
            event: "gotTabs",
            tabs: tabs
        });
    });
});

// Window Mapper
// ------------------------------------------------------------------------
//
function notifyWindowAdded(window) {
    if (!window.id) {
        console.warn("Cannot notify creation of window without id");
        return;
    }

    port.postMessage({
        subsystem: "windows",
        event: "added",
        browserId: window.id
    });
}

chrome.windows.onCreated.addListener(function (window) {
    notifyWindowAdded(window);
});

chrome.windows.onRemoved.addListener(function (windowId) {
    port.postMessage({
        subsystem: "windows",
        event: "removed",
        browserId: windowId
    });
});

// when the currently active tab changes
chrome.tabs.onActivated.addListener(function (activeInfo) {
    // maybe we should just keep track of all tabs, e.g. in Window have QHash<int tabId, Tab* > and just update them
    chrome.tabs.get(activeInfo.tabId, function (tab) {
        port.postMessage({
            subsystem: "windows",
            event: "update",
            browserId: tab.windowId,
            title: tab.title,
            activeTabId: tab.id,
            activeTabUrl: tab.url,
            audible: tab.audible, // FIXME this should be whether the window is audible not if the current tab is audible
            muted: tab.mutedInfo
        });
    });
});

chrome.tabs.onUpdated.addListener(function (tabId, changeInfo, tab) {
    if (!tab.active || !tab.windowId) {
        return;
    }

    var payload = {
        subsystem: "windows",
        event: "update",
        browserId: tab.windowId
    };

    if (changeInfo.title) {
        payload.title = changeInfo.title;
    }
    if (changeInfo.url) {
        payload.activeTabUrl = changeInfo.url;
    }
    if (changeInfo.audible) {
        payload.audible = changeInfo.audible;
    }
    if (changeInfo.mutedInfo) {
        payload.muted = changeInfo.mutedInfo;
    }

    port.postMessage(payload);
});

addCallback("windows", "getAll", function (message) {
    chrome.windows.getAll({
        populate: true // TODO needed for just the id?
    }, function (windows) {
        windows.forEach(function (window) {
            notifyWindowAdded(window)
        });
    });
});

// maps a given window resolve request to a tab (key: window id, value: tab id)
var resolverTabs = {};

addCallback("windows", "resolve", function (message) {
    var browserId = message.browserId;

    // now open a new tab with a special page that will set its title to the browser id we pass in
    // this we can then detect from the host and generate a mapping. Yuck.

    chrome.tabs.create({
        windowId: browserId,
        active: true,
        url: chrome.runtime.getURL("windowmapper.htm") + "#" + Number(browserId)
    }, function (tab) {
        resolverTabs[browserId] = tab.id;
    });
});

addCallback("windows", "resolved", function (message) {
    var browserId = message.browserId;

    // now close the tab again
    var tabId = resolverTabs[browserId];
    if (!tabId) {
        console.warn("Got told that resolving", browserId, "succeeded but we don't actually know the tab that did it?!");
        return;
    }

    chrome.tabs.remove(tabId, function () {
        delete resolverTabs[browserId];
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

connectHost();

addRuntimeCallback("settings", "changed", function () {
    // we could also just reload our extension :)
    // but this also causes the settings dialog to quit
    //chrome.runtime.reload();
    sendSettings();
});

port.onMessage.addListener(function (message) {
    var subsystem = message.subsystem;
    var action = message.action;

    if (!subsystem || !action) {
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

  console.log("Disconnected", error);

  chrome.notifications.create(null, {
      type: "basic",
      title: "Plasma Browser Integration Error",
      message: "The native host disconnected unexpectedly: " + (error ? error.message : "Unknown error"),
      iconUrl: "icons/sad-face-128.png"
  });

  // TODO crash recursion guard
  connectHost();
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
