/*
    Copyright (C) 2022 Kai Uwe Broulik <kde@broulik.de>

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

// NOTE keep in sync with manifest.json for Firefox.
self.importScripts(
    "constants.js",
    "utils.js",
    "extension-utils.js",

    "extension-kdeconnect.js",
    "extension-mpris.js",
    "extension-downloads.js",
    "extension-tabsrunner.js",
    "extension-purpose.js",
    "extension-historyrunner.js",

    "extension.js"
);
