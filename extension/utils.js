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

class SettingsUtils {
    static storage() {
        return (IS_FIREFOX ? chrome.storage.local : chrome.storage.sync);
    }

    static get() {
        return new Promise((resolve, reject) => {
            SettingsUtils.storage().get(DEFAULT_EXTENSION_SETTINGS, (items) => {
                const error = chrome.runtime.lastError;
                if (error) {
                    return reject(error);
                }

                resolve(items);
            });
        });
    }

    static set(settings) {
        return new Promise((resolve, reject) => {
            try {
                SettingsUtils.storage().set(settings, () => {
                    const error = chrome.runtime.lastError;
                    if (error) {
                        return reject(error);
                    }

                    resolve();
                });
            } catch (e) {
                reject(e);
            }
        });
    }
}


