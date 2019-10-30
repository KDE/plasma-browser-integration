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

class TabUtils {
    // Gets the currently viewed tab
    static getCurrentTab() {
        return new Promise((resolve, reject) => {
            chrome.tabs.query({
                active: true,
                currentWindow: true
            }, (tabs) => {
                const error = chrome.runtime.lastError;
                if (error) {
                    return reject(error.message);
                }

                const tab = tabs[0];
                if (!tab) {
                    return reject("NO_TAB");
                }

                resolve(tab);
            });
        });
    }

    // Gets the URLs of the currently viewed tab including all of its iframes
    static getCurrentTabFramesUrls() {
        return new Promise((resolve, reject) => {
            TabUtils.getCurrentTab().then((tab) => {
                chrome.tabs.executeScript({
                    allFrames: true, // so we also catch iframe videos
                    code: `window.location.href`,
                    runAt: "document_start"
                }, (result) => {
                    const error = chrome.runtime.lastError;
                    if (error) {
                        return reject(error.message);
                    }

                    resolve(result);
                });
            });
        });
    }
};

class MPrisBlocker {
    getAllowed() {
        return new Promise((resolve, reject) => {
            Promise.all([
                SettingsUtils.get(),
                TabUtils.getCurrentTabFramesUrls()
            ]).then((result) => {

                const settings = result[0];
                const currentUrls = result[1];

                const mprisSettings = settings.mpris;
                if (!mprisSettings.enabled) {
                    return reject("MPRIS_DISABLED");
                }

                if (!currentUrls) { // can this happen?
                    return reject("NO_URLS");
                }

                const origins = currentUrls.map((url) => {
                    try {
                        return new URL(url).origin;
                    } catch (e) {
                        console.warn("Invalid url", url);
                        return "";
                    }
                }).filter((origin) => {
                    return !!origin;
                });

                if (origins.length === 0) {
                    return reject("NO_ORIGINS");
                }

                const uniqueOrigins = [...new Set(origins)];

                const websiteSettings = mprisSettings.websiteSettings || {};

                let response = {
                    origins: {},
                    mprisSettings
                };

                for (const origin of uniqueOrigins) {
                    let allowed = true;
                    if (typeof MPRIS_WEBSITE_SETTINGS[origin] === "boolean") {
                        allowed = MPRIS_WEBSITE_SETTINGS[origin];
                    }
                    if (typeof websiteSettings[origin] === "boolean") {
                        allowed = websiteSettings[origin];
                    }

                    response.origins[origin] = allowed;
                }

                resolve(response);

            }, reject);
        });
    }

    setAllowed(origin, allowed) {
        return SettingsUtils.get().then((settings) => {
            const mprisSettings = settings.mpris;
            if (!mprisSettings.enabled) {
                return reject("MPRIS_DISABLED");
            }

            let websiteSettings = mprisSettings.websiteSettings || {};

            let implicitAllowed = true;
            if (typeof MPRIS_WEBSITE_SETTINGS[origin] === "boolean") {
                implicitAllowed = MPRIS_WEBSITE_SETTINGS[origin];
            }

            if (allowed !== implicitAllowed) {
                websiteSettings[origin] = allowed;
            } else {
                delete websiteSettings[origin];
            }

            mprisSettings.websiteSettings = websiteSettings;

            return SettingsUtils.set({
                mpris: mprisSettings
            });
        });
    }
};

class Itinerary {
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

                    this._doExtract("Pdf", result);
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

                this._doExtract("Html", result);
            });
        });
    }

    _doExtract(type, data) {
        return new Promise((resolve, reject) => {
            sendMessage("itinerary", "extract", {
                type,
                data
            }).then((result) => {
                if (!result.success) {
                    // TODO print error?
                    return;
                }

                this.buildUi(result.data);
            });
        });
    }

    buildUi(result) {
        if (!result || result.length === 0) {
            return;
        }

        const foundTypes = result.map((item) => {
            return item["@type"];
        });

        // is joined together as a string so I don't always have to expand the Array in the developer console
        console.log("Itinerary extractor found types:", foundTypes.join(",").substr(0,100));

        const filteredResult = result.filter((item) => {
            return SUPPORTED_ITINERARY_TYPES.includes(item["@type"]);
        });

        // Likely an overview page, not much practical use
        if (filteredResult.length > MAXIMUM_ITINERARY_TYPE_OCCURRENCES) {
            return;
        }

        // HACK prefer the result with most keys, likely to have more useful data
        // Ideally KItinerary filtered out obvious garbage like a Restaurant with just a name and nothing else
        filteredResult.sort((a, b) => {
            return Object.keys(b).length - Object.keys(a).length;
        });

        const data = filteredResult[0];
        if (!data) {
            return;
        }

        // There's some content, hide dummy placeholder
        document.getElementById("dummy-main").classList.add("hidden");

        const infoElement = document.querySelector(`.itinerary-info[data-itinerary-context*='${data["@type"]}'`);
        infoElement.classList.remove("hidden");

        // All of this below is pretty ridiculous and should be replaced by a proper HTML templating engine :)
        const elements = infoElement.querySelectorAll("[data-prop]");
        for (const element of elements) {
            const keys = element.dataset.prop.split(",");

            const values = keys.map((key) => {
                const segments = key.split(".");
                let depth = 0;
                let value = data;

                while (depth < segments.length) {
                    const key = segments[depth];
                    value = value[key];
                    ++depth;
                    if (!value) {
                        return undefined;
                    }
                }

                return value;
            });
            const value = values[0];

            if (!values.some((value) => {
                return !!value;
            })) {
                element.classList.add("hidden");
                continue;
            }

            switch (element.tagName) {
            case "IMG":
                element.src = value;
                break;
            default:
                const formatter = element.dataset.formatter;
                switch (formatter) {

                case "daterange":
                    // TODO check if there's only one date
                    const now = new Date();
                    const startDate = new Date(values[0]);
                    const endDate = new Date(values[1]);

                    // TODO pretty format when end on same as start, no year if same and current etc etc etc

                    const formattedDates = values.map((value) => {
                        return new Date(value).toLocaleString(navigator.language, {
                            weekday: 'short', year: 'numeric', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit'
                        });
                    });

                    element.innerText = `${formattedDates[0]} to ${formattedDates[1]}`;


                    /*let startDateFormat = {
                        weekday: "short"
                        day: "numeric",
                        month: "short"
                    };
                    let endDateFormat = {

                    };

                    if (startDate.getFullYear() !== endDate.getFullYear()
                        || startDate.getFullYear() !== now.getFullYear()
                        || endDate.getFullYear() !== now.getFullYear()) {
                        dateFormat.year = "numeric";
                    }

                    if (startDate.getHours() !== 0 || startDate.getMinutes() !== 0 || startDate.getSeconds() !== 0
                        || endDate.getHours() !== 0 || endDate.getMinutes() !== 0 || endDate.getSeconds !== 0) {
                        dateFormat.hour = "2-digit";
                        dateFormat.minute = "2-digit";
                    }


                    // TODO handle if there's only one date
                    element.innerText = `${formattedDates[0]} to ${formattedDates[1]}`;*/
                    break;

                case "place":
                    const address = value.address;
                    switch (address["@type"]) {
                    case "PostalAddress":
                        // TODO geo url or OSM or whatever
                        // FIXME
                        element.innerText = `${address.streetAddress}, ${address.postalCode}, ${address.addressLocality} (${address.addressCountry})`;
                        break;
                    }

                    break;
                case "address":
                    switch (value["@type"]) {
                    case "PostalAddress":
                        // TODO geo url or OSM or whatever
                        // FIXME
                        element.innerText = `${value.streetAddress}\n${value.postalCode}, ${value.addressLocality} (${value.addressCountry})`;
                        break;
                    }

                    break;

                case "phonelink": {
                    let link = document.createElement("a");
                    link.href = "tel:" + value;
                    link.target = "_blank";
                    link.innerText = value; // TODO pretty format phone number
                    link.addEventListener("click", (e) => {
                        e.preventDefault();
                        alert("Call " + link.href);
                    });
                    // TODO clear element before
                    element.appendChild(link);
                    break;
                }

                case "emaillink": {
                    let link = document.createElement("a");
                    link.href = "mailto:" + value;
                    link.target = "_blank";
                    link.innerText = value;
                    element.appendChild(link);
                    break;
                }

                case "background-image":
                    let backgroundUrl = value;
                    if (backgroundUrl.startsWith("//")) {
                        // FIXME resolve properly based on current website URL!
                        backgroundUrl = "https:" + backgroundUrl;
                    }
                    element.style.backgroundImage = `url('${backgroundUrl}')`;
                    break;

                case "noop":
                    break;
                default:
                    element.innerText = value;
                }
            }
        }

        const outOfContextElements = infoElement.querySelectorAll("[data-context]:not([data-context=" + data["@type"]);
        outOfContextElements.forEach((element) => {
            element.classList.add("hidden");
        });

        const actionsListElement = document.getElementById("itinerary-info-actions");

        const actions = data.potentialAction || [];
        for (const action of actions) {
            let listItem = document.createElement("li");

            let link = document.createElement("a");

            let text = action.result && action.result.name;
            if (!text) {
                switch (action["@type"]) {
                case "ReserveAction":
                    switch (data["@type"]) {
                    case "FoodEstablishment":
                    case "Restaurant":
                    // TODO rest
                        text = "Reserve a Table"; // FIXME i18n
                        break;
                    }

                    break;
                }
            }

            link.innerText = text;

            link.href = action.target;
            link.target = "_blank";

            listItem.appendChild(link);

            actionsListElement.appendChild(listItem);
        }

        document.querySelectorAll("[data-action]").forEach((element) => {
            element.addEventListener("click", (e) => {
                e.preventDefault();

                const action = element.dataset.action;

                switch (action) {
                case "add-to-itinerary":

                    /*sendMessage("itinerary", "run", {
                        path: ""
                    });*/

                    break;
                }
            });
        });
    }
};

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

                // There's some content, hide dummy placeholder
                document.getElementById("dummy-main").classList.add("hidden");
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

    // MPris blocker checkboxes
    const blocker = new MPrisBlocker();
    blocker.getAllowed().then((result) => {
        const origins = result.origins;

        if (Object.entries(origins).length === 0) { // "isEmpty"
            return;
        }

        // To keep media controls setting from always showing up, only show them, if:
        // - There is actually a player anywhere on this tab
        // or, since when mpris is disabled, there are never any players
        // - when media controls are disabled for any origin on this tab
        new Promise((resolve, reject) => {
            for (let origin in origins) {
                if (origins[origin] === false) {
                    return resolve("HAS_BLOCKED");
                }
            }

            TabUtils.getCurrentTab().then((tab) => {
                return sendMessage("mpris", "hasTabPlayer", {
                    tabId: tab.id
                });
            }).then((playerIds) => {
                if (playerIds.length > 0) {
                    return resolve("HAS_PLAYER");
                }

                reject("NO_PLAYER_NO_BLOCKED");
            });
        }).then(() => {
            // There's some content, hide dummy placeholder
            document.getElementById("dummy-main").classList.add("hidden");

            let blacklistInfoElement = document.querySelector(".mpris-blacklist-info");
            blacklistInfoElement.classList.remove("hidden");

            let originsListElement = blacklistInfoElement.querySelector("ul.mpris-blacklist-origins");

            for (const origin in origins) {
                const originAllowed = origins[origin];

                let blockListElement = document.createElement("li");

                let labelElement = document.createElement("label");
                labelElement.innerText = origin;

                let checkboxElement = document.createElement("input");
                checkboxElement.type = "checkbox";
                checkboxElement.checked = (originAllowed === true);
                checkboxElement.addEventListener("click", (e) => {
                    // Let us handle (un)checking the checkbox when setAllowed succeeds
                    e.preventDefault();

                    const allowed = checkboxElement.checked;
                    blocker.setAllowed(origin, allowed).then(() => {
                        checkboxElement.checked = allowed;
                    }, (err) => {
                        console.warn("Failed to change media controls settings:", err);
                    });
                });

                labelElement.insertBefore(checkboxElement, labelElement.firstChild);

                blockListElement.appendChild(labelElement);

                originsListElement.appendChild(blockListElement);
            }
        }, (err) => {
            console.log("Not showing media controls settings because", err);
        });
    }, (err) => {
        console.warn("Failed to check for whether media controls are blocked", err);
    });

    sendMessage("settings", "getSubsystemStatus").then((status) => {
        if (status && status.itinerary && status.itinerary.loaded) {
            new Itinerary().extract();
        }
    });
});
