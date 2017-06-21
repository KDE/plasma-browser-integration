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

DEFAULT_EXTENSION_SETTINGS = {
    mpris: {
        enabled: true
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
    slc: {
        enabled: true
    },
    incognito: {
        // somewhat buggy, only works when allow in incognito is on
        // and I'm actually tempted to remove this
        enabled: false
    },
    breezeScrollBars: {
        // this breaks pages in interesting ways, disable by default
        enabled: false
    }
};

IS_FIREFOX = (typeof InstallTrigger !== "undefined"); // heh.
