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

    static onChanged() {
        return chrome.storage.onChanged;
    }
}

class BlobReader {
    constructor(url) {
        this._url = url;
        this._timeout = -1;
        this._type = BlobReader.READ_TYPE_TEXT;
    }

    // oof...
    static get READ_TYPE_ARRAY_BUFFER() { return "ArrayBuffer"; }
    static get READ_TYPE_BINARY_STRING() { return "BinaryString"; }
    static get READ_TYPE_DATA_URL() { return "DataURL"; }
    static get READ_TYPE_TEXT() { return "Text"; }

    get timeout() {
        return this._timeout;
    }
    set timeout(timeout) {
        this._timeout = timeout;
    }

    get type() {
        return this._type;
    }
    set type(type) {
        this._type = type;
    }

    read() {
        return new Promise((resolve, reject) => {
            const xhr = new XMLHttpRequest();
            xhr.onreadystatechange = () => {
                if (xhr.readyState != 4) {
                    return;
                }

                if (!xhr.response) { // TODO check status code, too?
                    return reject("NO_RESPONSE");
                }

                const reader = new FileReader();
                reader.onloadend = () => {
                    if (reader.error) {
                        return reject(reader.error);
                    }

                    resolve(reader.result);
                };

                reader["readAs" + this._type](xhr.response);
            };

            xhr.open("GET", this._url);
            xhr.responseType = "blob";
            if (this._timeout > -1) {
                xhr.timeout = this._timeout;
            }
            xhr.send();
        });
    }
};
