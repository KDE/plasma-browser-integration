/*
    Copyright (C) 2020 Kai Uwe Broulik <kde@broulik.de>

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

function getFavicon(url) {
    return new Promise((resolve) => {
        // Specal favicon URL, needs "favicon" permission
        // see https://bugs.chromium.org/p/chromium/issues/detail?id=104102
        let faviconUrl = new URL(chrome.runtime.getURL("_favicon"));
        faviconUrl.searchParams.append("pageUrl", url);
        // TODO devicePixelRatio
        faviconUrl.searchParams.append("size", 32);

        fetch(faviconUrl.href, {
            // Unfortunately "only-if-cached" is only possible with "same-origin" mode
            cache: "force-cache"
        }).then((response) => {
            if (!response.ok) {
                return resolve();
            }

            response.blob().then((blob) => {
                let reader = new FileReader();
                reader.onloadend = function() {
                    resolve(reader.result);
                }
                reader.readAsDataURL(blob);
            }, (err) => {
                console.warn("Failed to read response of", faviconUrl.href, "as blob", err);
                resolve();
            });
        }, (err) => {
            console.warn("Failed to get favicon from", faviconUrl.href, err);
            resolve();
        });
    });
}

addCallback("historyrunner", "find", (message) => {
    const query = message.query;

    chrome.permissions.contains({
        permissions: ["history"]
    }, (granted) => {
        if (!granted) {
            sendPortMessage("historyrunner", "found", {
                query,
                error: "NO_PERMISSION"
            });
            return;
        }

        chrome.history.search({
            text: query,
            maxResults: 15,
            // By default searches only the past 24 hours but we want everything
            startTime: 0
        }, (results) => {
            let promises = [];

            // Collect open tabs for each history item URL to filter them out below
            results.forEach((result) => {
                promises.push(new Promise((resolve) => {
                    chrome.tabs.query({
                        url: result.url
                    }, (tabs) => {
                        if (chrome.runtime.lastError || !tabs) {
                            return resolve([]);
                        }

                        resolve(tabs);
                    });
                }));
            });

            Promise.all(promises).then((tabs) => {
                // Now filter out the entries with corresponding tabs we found earlier
                results = results.filter((result, index) => {
                    return tabs[index].length === 0;
                });

                // Now fetch all favicons from special favicon provider URL
                // There's no public API for this.
                // chrome://favicon/ works on Chrome "by accident", and for
                // Firefox' page-icon: scheme there is https://bugzilla.mozilla.org/show_bug.cgi?id=1315616
                if (IS_FIREFOX) {
                    return;
                }

                promises = [];
                results.forEach((result) => {
                    promises.push(getFavicon(result.url));
                });

                return Promise.all(promises);
            }).then((favicons) => {
                if (favicons) {
                    favicons.forEach((favicon, index) => {
                        if (favicon) {
                            results[index].favIconUrl = favicon;
                        }
                    });
                }
            }).then((faviconData) => {
                sendPortMessage("historyrunner", "found", {
                    query,
                    results
                });
            });

        });
    });
});

addCallback("historyrunner", "run", (message) => {
    const url = message.url;

    chrome.tabs.create({
        url
    });
});

addCallback("historyrunner", "requestPermission", () => {
    chrome.tabs.create({
        url: chrome.runtime.getURL("permission_request.html") + "?permission=history"
    });
});
