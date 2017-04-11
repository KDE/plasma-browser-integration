/*
    Copyright (C) 2017 Kai Uwe Broulik <kde@privat.broulik.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

console.log("HALLO?");

var port
var callbacks = [];

function addCallback(subsystem, action, callback)
{
    if (!callbacks[subsystem]) {
        callbacks[subsystem] = [];
    }
    callbacks[subsystem][action] = callback;
}

function connectHost() {
    port = chrome.runtime.connectNative("org.kde.plasma.browser_integration");
}

// KDE Connect
// ------------------------------------------------------------------------
//

var kdeConnectDefaultDeviceId = "";
var kdeConnectDefaultDeviceName = "";
var hasKdeConnectMenu = false;

chrome.contextMenus.onClicked.addListener(function (info) {
    if (info.menuItemId === "kdeconnect_page") {
        var url = info.linkUrl;
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
    console.log("devices chagned CB");
    if (message.defaultDeviceId) {
        kdeConnectDefaultDeviceId = message.defaultDeviceId
    }
    if (message.defaultDeviceName) {
        kdeConnectDefaultDeviceName = message.defaultDeviceName
    }

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
                contexts: ["link"],
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

var currentPlayerTab;

chrome.runtime.onMessage.addListener(function (request, sender, sendResponse) {
    if (request.subsystem === "mpris") {
        switch (request.action) {
        case "play":
            currentPlayerTab = sender.tab;
            console.log("TAB", currentPlayerTab);
            port.postMessage({subsystem: "mpris", event: "play", title: currentPlayerTab.title});
            break;
        case "pause":
            port.postMessage({subsystem: "mpris", event: "pause"});
            break;
        }
    }
});

// MISC
// ------------------------------------------------------------------------
//

chrome.windows.getAll({
    populate: true
}, function (windows) {
    console.log("CHROME WINS", windows);
});


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
                bytesReceived: download.bytesReceived
            };

            port.postMessage({subsystem: "downloads", event: "update", id: download.id, payload: payload});
        });
    });
}, 1000);

//chrome.downloads.setShelfEnabled(false);

chrome.downloads.onCreated.addListener(function (download) {
    // don't bother telling us about completed downloads...
    // otherwise on browser startup we'll spawn a gazillion download progress notification
    if (download.state === "complete" || download.state === "interrupted") {
        return;
    }

    var payload = {
        url: download.url,
        finalUrl: download.finalUrl,
        destination: download.filename,
        startTime: download.startTime,

        totalBytes: download.totalBytes,
        bytesReceived: download.bytesReceived
    };

    activeDownloads.push(download.id);

    port.postMessage({subsystem: "downloads", event: "created", id: download.id, payload: payload});
});

chrome.downloads.onChanged.addListener(function (delta) {
    if (activeDownloads.indexOf(delta.id) === -1) {
        console.log("ignoring download", delta.id, "that we didn't track");
    }

    var payload = {};

    if (delta.url) {
        payload.url = delta.url.current;
    }

    if (delta.filename) {
        payload.destination = delta.filename.current;
    }

    if (delta.state) {
        payload.state = delta.state.current;
    }

    if (delta.error) {
        payload.error = delta.error.current;
    }

    port.postMessage({subsystem: "downloads", event: "update", id: delta.id, payload: payload});
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



// System
// ------------------------------------------------------------------------
//

connectHost();

port.onMessage.addListener(function (message) {
    console.log("PORT MESSAGE", message);

    var subsystem = message.subsystem;
    var action = message.action;

    if (!subsystem || !action) {
        return;
    }

    callbacks[subsystem][action](message.payload);
});

port.onDisconnect.addListener(function() {
  var error = chrome.runtime.lastError;

  console.log("Disconnected", error);

  chrome.notifications.create(null, {
      type: "basic",
      title: "Plasma Chrome Integration Error",
      message: "The native host disconnected unexpectedly: " + error.message,
      iconUrl: "icons/sad-face-128.png"
  });

  // TODO crash recursion guard
  connectHost();
});
