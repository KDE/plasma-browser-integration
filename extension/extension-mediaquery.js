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

let currentColorScheme = "";
addCallback("mediaquery", "currentScheme", (message) => {
    currentColorScheme = message.currentScheme;

    // Unfortunately have to manually send this to all tabs
    chrome.tabs.query({}, (tabs) => {
        tabs.forEach((tab) => {
            chrome.tabs.sendMessage(tab.id, {
                subsystem: "mediaquery",
                action: "currentScheme",
                payload: currentColorScheme
            });
        });
    });
});

addRuntimeCallback("mediaquery", "getCurrentScheme", (message, sender, action, sendReply) => {
    return Promise.resolve(currentColorScheme);
});
