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

IS_FIREFOX = (navigator && navigator.userAgent || '').toLowerCase().match(/(?:firefox|fxios)/) !== null;

DEFAULT_EXTENSION_SETTINGS = {
    mpris: {
        enabled: true,
        websiteSettings: {}
    },
    mprisMediaSessions: {
        enabled: true
    },
    kdeconnect: {
        enabled: true
    },
    downloads: {
        enabled: true,
        addToRecentDocuments: true,
        saveOriginUrl: false,
        // Chrome inhibits by itself.
        inhibitSuspend: IS_FIREFOX,
    },
    tabsrunner: {
        enabled: true
    },
    historyrunner: {
        enabled: true
    },
    purpose: {
        enabled: true
    },
    breezeScrollBars: {
        // this breaks pages in interesting ways, disable by default
        enabled: false
    }
};

// NOTE if you change this, make sure to adjust the error message shown in action_popup.html
SUPPORTED_PLATFORMS = ["linux", "openbsd", "freebsd"];

// Default MPRIS settings for websites
const MPRIS_WEBSITE_SETTINGS = {
    //"https://www.example.com": false
    // Disable for known conferencing solutions.
    // It may interfere with webcam or audio and isn't designed for this anyway.
    "https://meet.google.com": false, // Bug 435418
    "https://getmibo.com": false,
    "https://zoom.us": false, // Bug 435100
    "https://teams.microsoft.com": false, // Bug 503965
    "https://www.nicovideo.jp": false, // Bug 493809
};
