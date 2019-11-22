/*
    Copyright (C) 2019 Kai Uwe Broulik <kde@privat.broulik.de>

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

// The list of service is in the extension so we can easily add more services
// without having to rely on updates for the host
const websiteServices = {
    // Messenger services
    telegram: {
        url: "https://web.telegram.org/*"
    },
    whatsapp: {
        url: "https://web.whatsapp.com/*"
    },
    threema: {
        url: "https://web.threema.ch/*"
    },
    hangouts: {
        url: "https://hangouts.google.com/*"
    },

    // Other communication stuff
    gmail: {
        url: "https://mail.google.com/mail/*"
    },

    // Music players
    /*spotify: {
        url: "https://open.spotify.com/*"
    }*/
};

addCallback("notificationfilter", "checkService", (message) => {

    const service = message.service;

    const serviceInfo = websiteServices[service];
    if (!serviceInfo) {
        sendPortMessage("notificationfilter", "unknownService", {
            service
        });
        return;
    }

    chrome.tabs.query({
        url: serviceInfo.url
    }, (tabs) => {
        const tab = tabs[0];
        if (chrome.runtime.lastError || !tab || tab.incognito) {
            sendPortMessage("notificationfilter", "gotService", {
                service,
                running: false
            });
            return;
        }

        // Now check Notification.permission
        chrome.tabs.executeScript(tab.id, {
            code: "Notification.permission"
        }, (result) => {
            if (chrome.runtime.lastError) {
                sendPortMessage("notificationfilter", "gotService", {
                    service,
                    running: true
                });
                return;
            }

            result = result[0];

            sendPortMessage("notificationfilter", "gotService", {
                service,
                running: true,
                notificationPermission: String(result)
            });
        });

    });

});
