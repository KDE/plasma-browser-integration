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

DEFAULT_EXTENSION_SETTINGS = {
    mpris: {
        enabled: true
    },
    mprisMediaSessions: {
        enabled: false
    },
    kdeconnect: {
        enabled: true
    },
    downloads: {
        enabled: true
    },
    tabsrunner: {
        enabled: true
    },
    breezeScrollBars: {
        // this breaks pages in interesting ways, disable by default
        enabled: false
    }
};

IS_FIREFOX = (typeof InstallTrigger !== "undefined"); // heh.

SUPPORTED_PLATFORMS = ["linux", "openbsd"];
