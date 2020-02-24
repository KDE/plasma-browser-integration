/*
    Copyright (C) 2019-2020 Kai Uwe Broulik <kde@privat.broulik.de>

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

class Itinerary {
    constructor(args) {
        args = args || {};
        this._status = args.status || {};
        this._config = args.config || {};
        this._kdeConnectDevices = args.kdeConnectDevices || [];
    }

    extract() {
        TabUtils.getCurrentTab().then((tab) => {
            // Extract currently viewed PDF, e.g. if vieweing boarding pass PDF in internal PDF viewer
            if (new URL(tab.url).pathname.toLowerCase().endsWith(".pdf")) {
                const reader = new BlobReader(tab.url);
                reader.type = BlobReader.READ_TYPE_DATA_URL;
                reader.read().then((result) => {
                    // Limit from ExtractorEngine, plus 30% overhead of being base64 encoded
                    if (result.length > 4000000 * 1.3) {
                        return;
                    }

                    this._doExtract("Pdf", tab.id, result);
                });
                return;
            }

            // Extract website HTML
            chrome.tabs.executeScript({
                allFrames: false,
                code: `document.documentElement.outerHTML`,
                runAt: "document_end"
            }, (result) => {
                if (chrome.runtime.lastError) {
                    return;
                }

                result = result[0];
                if (!result) {
                    return;
                }

                this._doExtract("Html", tab.id, result);
            });
        });
    }

    _doExtract(type, tabId, data) {
        this._data = undefined;

        return new Promise((resolve, reject) => {
            sendMessage("itinerary", "extract", {
                type,
                // since "sender" will be the action_popup but we want to know what browser tab this is for
                tabId,
                data
            }).then((result) => {
                if (!result.success) {
                    // TODO print error?
                    console.warn("Itinerary: extractor failed");
                    return;
                }

                this._data = result.data;
                this.buildUi(this._data);
            });
        });
    }

    buildUi(result) {
        if (!result || result.length === 0) {
            console.log("Itinerary: extractor found nothing");
            return;
        }

        const foundTypes = result.map((item) => {
            return item["@type"];
        });

        // is joined together as a string so I don't always have to expand the Array in the developer console
        console.log("Itinerary: extractor found types:", foundTypes.join(",").substr(0,100));
        console.log("Itinerary: backend status info", this._status);

        const filteredResult = result.filter((item) => {
            return SUPPORTED_ITINERARY_TYPES.includes(item["@type"]);
        });

        // Likely an overview page, not much practical use
        if (filteredResult.length > MAXIMUM_ITINERARY_TYPE_OCCURRENCES) {
            console.log("Itinerary: There are", filteredResult.length, "items on this page. This is possibly an overview/list page, ignoring.");
            return;
        }

        // HACK prefer the result with most keys, likely to have more useful data
        // Ideally KItinerary filtered out obvious garbage like a Restaurant with just a name and nothing else
        filteredResult.sort((a, b) => {
            return Object.keys(b).length - Object.keys(a).length;
        });

        if (filteredResult.length === 0) {
            console.log("Itinerary: None of the types found are supported by us");
            return;
        }

        // Load the UI script on demand
        const itineraryUiScript = document.createElement("script");
        itineraryUiScript.src = "ui_itinerary.js";
        itineraryUiScript.onerror = (e) => {
            console.error("Failed to load Itinerary UI! Make sure to generate the files from 'ui-templates' directory");
        };
        itineraryUiScript.onload = () => {
            // TODO try catch and show an error to the user when this is fails and ask to file a bug report?
            const generatedHtml = ui_itinerary({
                data: filteredResult,
                types: foundTypes,
                status: this._status,
                config: this._config,
                kdeConnectDevices: this._kdeConnectDevices
            });

            // There's now some content, hide dummy placeholder
            document.getElementById("dummy-main").classList.add("hidden");

            const container = document.getElementById("itinerary-container");
            container.innerHTML = generatedHtml;
            container.classList.remove("hidden");

            container.querySelectorAll("a[data-itinerary-action]").forEach((link) => {
                link.addEventListener("click", (e) => {
                    e.preventDefault();

                    const action = link.href.split("#")[1];

                    let args = {};
                    Object.assign(args, link.dataset);

                    this._invokeAction(action, args).then((reply) => {
                        console.log("Invoked action", reply);
                    }, (err) => {
                        console.warn("Failed to invoke", err);
                    });
                });
            });
        };
        document.head.appendChild(itineraryUiScript);
    }

    _invokeAction(action, args) {
        console.log("Invoke Itinerary action", action, args);

        switch (action) {
        case "open-location":
            console.log("OPEN LOC", args);
            return sendMessage("itinerary", "openLocation", {
                latitude: args.geoLat,
                longitude: args.geoLon,
                address: args.address
            });
        case "kdeconnect-location":
            return sendMessage("itinerary", "sendLocationToDevice", {
                deviceId: args.deviceId,
                latitude: args.geoLat,
                longitude: args.geoLon,
                address: args.address
            });

        case "kdeconnect-call":
            return sendMessage("itinerary", "callOnDevice", {
                deviceId: args.deviceId,
                phoneNumber: args.phoneNumber
            });

        case "calendar":
            return sendMessage("itinerary","addToCalendar", {
                data: this._data
            });
        case "kdeconnect": {
            return sendMessage("itinerary", "sendToDevice", {
                data: this._data,
                deviceId: args.deviceId
            });
        }
        case "itinerary":
            return TabUtils.getCurrentTab().then((tab) => {
                return sendMessage("itinerary", "openInItinerary", {
                    // FIXME For Websites we should probably send the data?
                    // Since the extension also sees stuff added asynchronously through AJAX.
                    // However, for ticket PDFs we want the file so Itinerary can attach it
                    url: tab.url,
                    //data: this._data
                });
            });
            break;
        case "workbench":
            return TabUtils.getCurrentTab().then((tab) => {
                return sendMessage("itinerary", "openInWorkbench", {
                    url: tab.url
                });
            });
        default:
            throw new Error("Unknown action " + action);
        }
    }
};
