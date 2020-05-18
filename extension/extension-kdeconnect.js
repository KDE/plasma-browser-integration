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

var kdeConnectMenuIdPrefix = "kdeconnect_page_";
var kdeConnectDevices = {};

chrome.contextMenus.onClicked.addListener(function (info) {
    if (!info.menuItemId.startsWith(kdeConnectMenuIdPrefix)) {
        return;
    }

    const deviceId = info.menuItemId.substr(info.menuItemId.indexOf("@") + 1);

    var url = info.linkUrl || info.srcUrl || info.pageUrl;
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
    let deviceId = message.id;
    let name = message.name;
    let type = message.type;

    let props = {
        id: kdeConnectMenuIdPrefix + "open@" + deviceId,
        contexts: ["link", "page", "image", "audio", "video"],
        title: chrome.i18n.getMessage("kdeconnect_open_device", name),
        targetUrlPatterns: [
            "http://*/*", "https://*/*"
        ]
    };

    if (IS_FIREFOX) {
        let iconName = "";
        switch (type) {
            case "smartphone":
            case "phone":
                iconName = "smartphone-symbolic";
                break;
            case "tablet":
                iconName = "tablet-symbolic";
                break;
            case "desktop":
            case "tv": // at this size you can't really tell desktop monitor icon from a TV
                iconName = "computer-symbolic";
                break;
            case "laptop":
                iconName = "computer-laptop-symbolic";
                break;
        }

        if (iconName) {
            props.icons = {
                "16": "icons/" + iconName + ".svg"
            };
        }
    }

    chrome.contextMenus.create(props);

    props = {
        id: kdeConnectMenuIdPrefix + "call@" + deviceId,
        contexts: ["link"],
        title: chrome.i18n.getMessage("kdeconnect_call_device", name),
        targetUrlPatterns: [
            "tel:*"
        ]
    };

    if (IS_FIREFOX) {
        props.icons = {
            "16": "icons/call-start-symbolic.svg"
        };
    }

    chrome.contextMenus.create(props);

    kdeConnectDevices[deviceId] = {
        name, type
    };
});

addCallback("kdeconnect", "deviceRemoved", function(message) {
    let deviceId = message.id;

    if (!kdeConnectDevices[deviceId]) {
        return;
    }

    delete kdeConnectDevices[deviceId];
    chrome.contextMenus.remove(kdeConnectMenuIdPrefix + "open@" + deviceId);
    chrome.contextMenus.remove(kdeConnectMenuIdPrefix + "call@" + deviceId);
});
