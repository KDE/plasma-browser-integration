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

document.addEventListener("DOMContentLoaded", () => {

    sendMessage("browserAction", "getStatus").then((status) => {

        switch (status.portStatus) {
        case "UNSUPPORTED_OS":
            document.getElementById("unsupported_os_error").classList.remove("hidden");
            break;

        case "STARTUP_FAILED":
            document.getElementById("startup_error").classList.remove("hidden");
            break;

        default:
            document.getElementById("main").classList.remove("hidden");

            let errorText = status.portLastErrorMessage;
            if (errorText === "UNKNOWN") {
                errorText = chrome.i18n.getMessage("general_error_unknown");
            }

            if (errorText) {
                document.getElementById("runtime_error_text").innerText = errorText;
                document.getElementById("runtime_error").classList.remove("hidden");
            }

            break;
        }

        // HACK so the extension can tell we closed, see "browserAction" "ready" callback in extension.js
        chrome.runtime.onConnect.addListener((port) => {
            if (port.name !== "browserActionPort") {
                return;
            }

            // do we need to do something with the port here?
        });
        sendMessage("browserAction", "ready");
    });

});
