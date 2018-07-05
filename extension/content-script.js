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

var callbacks = {};

function addCallback(subsystem, action, callback)
{
    if (!callbacks[subsystem]) {
        callbacks[subsystem] = {};
    }
    callbacks[subsystem][action] = callback;
}

function sendMessage(subsystem, action, payload)
{
    (chrome.extension.sendMessage || browser.runtime.sendMessage)({
        subsystem: subsystem,
        action: action,
        payload: payload
    });
}

function executeScript(script) {
    var element = document.createElement('script');
    element.innerHTML = '('+ script +')();';
    (document.body || document.head || document.documentElement).appendChild(element);
    // We need to remove the script tag after inserting or else websites relying on the order of items in
    // document.getElementsByTagName("script") will break (looking at you, Google Hangouts)
    element.parentNode.removeChild(element);
}

chrome.runtime.onMessage.addListener(function (message, sender) {
    // TODO do something with sender (check privilige or whatever)

    var subsystem = message.subsystem;
    var action = message.action;

    if (!subsystem || !action) {
        return;
    }

    if (callbacks[subsystem] && callbacks[subsystem][action]) {
        callbacks[subsystem][action](message.payload);
    }
});

// BREEZE SCROLL BARS
// ------------------------------------------------------------------------
//
if (!IS_FIREFOX) {
    chrome.storage.sync.get(DEFAULT_EXTENSION_SETTINGS, function (items) {
        if (items.breezeScrollBars.enabled) {
            var linkTag = document.createElement("link");
            linkTag.rel = "stylesheet";
            linkTag.href =  chrome.extension.getURL("breeze-scroll-bars.css");
            (document.head || document.documentElement).appendChild(linkTag);
        }
    });
}

// MPRIS
// ------------------------------------------------------------------------
//

// we give our transfer div a "random id" for privacy
// from https://stackoverflow.com/questions/105034/create-guid-uuid-in-javascript
var mediaSessionsTransferDivId ='xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
    var r = Math.random()*16|0, v = c == 'x' ? r : (r&0x3|0x8);
    return v.toString(16);
});

// also give the function a "random" name as we have to have it in global scope to be able
// to invoke callbacks from outside, UUID might start with a number, so prepend something
var mediaSessionsClassName = "f" + mediaSessionsTransferDivId.replace(/-/g, "");

var activePlayer;
var playerMetadata = {};
var playerCallbacks = [];

var players = [];

var pendingSeekingUpdate = 0;

addCallback("mpris", "play", function () {
    playerPlay();
});

addCallback("mpris", "pause", function () {
    playerPause();
});

addCallback("mpris", "playPause", function () {
    if (activePlayer) {
        if (activePlayer.paused) { // TODO take into account media sessions playback state
            playerPlay();
        } else {
            playerPause();
        }
    }
});

// there's no dedicated "stop", simulate it be rewinding and reloading
addCallback("mpris", "stop", function () {
    if (activePlayer) {
        activePlayer.pause();
        activePlayer.currentTime = 0;
        // calling load() now as is suggested in some "how to fake video Stop" code snippets
        // utterly breaks stremaing sites
        //activePlayer.load();

        // needs to be delayed slightly otherwise we pause(), then send "stopped", and only after that
        // the "paused" signal is handled and we end up in Paused instead of Stopped state
        setTimeout(function() {
            sendMessage("mpris", "stopped");
        }, 1);
    }
});

addCallback("mpris", "next", function () {
    if (playerCallbacks.indexOf("nexttrack") > -1) {
        executeScript(`
            function() {
                try {
                    ${mediaSessionsClassName}.executeCallback("nexttrack");
                } catch (e) {
                    console.warn("Exception executing 'nexttrack' media sessions callback", e);
                }
            }
        `);
    }
});

addCallback("mpris", "previous", function () {
    if (playerCallbacks.indexOf("previoustrack") > -1) {
        executeScript(`
            function() {
                try {
                    ${mediaSessionsClassName}.executeCallback("previoustrack");
                } catch (e) {
                    console.warn("Exception executing 'previoustrack' media sessions callback", e);
                }
            }
        `);
    }
});

addCallback("mpris", "setPosition", function (message) {
    if (activePlayer) {
        activePlayer.currentTime = message.position;
    }
});

addCallback("mpris", "setPlaybackRate", function (message) {
    if (activePlayer) {
        activePlayer.playbackRate = message.playbackRate;
    }
});

addCallback("mpris", "setVolume", function (message) {
    if (activePlayer) {
        activePlayer.volume = message.volume;
    }
});

addCallback("mpris", "setLoop", function (message) {
    if (activePlayer) {
        activePlayer.loop = message.loop;
    }
});

function playerPlaying(player) {
    setPlayerActive(player);
}

function playerPaused(player) {
    sendPlayerInfo(player, "paused");
}

function setPlayerActive(player) {
    // Ignore short sounds, they are most likely a chat notification sound
    // but still allow when undetermined (e.g. video stream)
    if (!isNaN(player.duration) && player.duration > 0 && player.duration < 5) {
        return;
    }

    activePlayer = player;

    // when playback starts, send along metadata
    // a website might have set Media Sessions metadata prior to playing
    // and then we would have ignored the metadata signal because there was no player
    sendMessage("mpris", "playing", {
        duration: player.duration,
        currentTime: player.currentTime,
        playbackRate: player.playbackRate,
        volume: player.volume,
        loop: player.loop,
        metadata: playerMetadata,
        callbacks: playerCallbacks
    });
}

function sendPlayerInfo(player, event, payload) {
    if (player != activePlayer) {
        return;
    }

    sendMessage("mpris", event, payload);
}

function registerPlayer(player) {
    if (players.indexOf(player) > -1) {
        //console.log("Already know", player);
        return;
    }

    // auto-playing player, become active right away
    if (!player.paused) {
        playerPlaying(player);
    }
    player.addEventListener("play", function () {
        playerPlaying(player);
    });

    player.addEventListener("pause", function () {
        playerPaused(player);
    });

    // what about "stalled" event?
    player.addEventListener("waiting", function () {
        sendPlayerInfo(player, "waiting");
    });

    // opposite of "waiting", we finished buffering enough
    // only if we are playing, though, should we set playback state back to playing
    player.addEventListener("canplay", function () {
        if (!player.paused) {
            sendPlayerInfo(player, "canplay");
        }
    });

    player.addEventListener("timeupdate", function () {
        sendPlayerInfo(player, "timeupdate", {
            currentTime: player.currentTime
        });
    });

    player.addEventListener("ratechange", function () {
        sendPlayerInfo(player, "ratechange", {
            playbackRate: player.playbackRate
        });
    });

    // TODO use player.seekable for determining whether we can seek?
    player.addEventListener("durationchange", function () {
        sendPlayerInfo(player, "duration", {
            duration: player.duration
        });
    });

    player.addEventListener("seeking", function () {
        if (pendingSeekingUpdate) {
            return;
        }

        // Compress "seeking" signals, this is invoked continuously as the user drags the slider
        pendingSeekingUpdate = setTimeout(function() {
            pendingSeekingUpdate = 0;
        }, 250);

        sendPlayerInfo(player, "seeking", {
            currentTime: player.currentTime
        });
    });

    player.addEventListener("seeked", function () {
        sendPlayerInfo(player, "seeked", {
            currentTime: player.currentTime
        });
    });

    player.addEventListener("volumechange", function () {
        sendPlayerInfo(player, "volumechange", {
            volume: player.volume
        });
    });

    // TODO remove it again when it goes away
    players.push(player);
}

function registerAllPlayers() {
    var players = document.querySelectorAll("video,audio");
    players.forEach(registerPlayer);
}

function playerPlay() {
    // if a media sessions callback is registered, it takes precedence over us manually messing with the player
    if (playerCallbacks.indexOf("play") > -1) {
        executeScript(`
            function() {
                try {
                    ${mediaSessionsClassName}.executeCallback("play");
                } catch (e) {
                    console.warn("Exception executing 'play' media sessions callback", e);
                }
            }
        `);
    } else if (activePlayer) {
        activePlayer.play();
    }
}

function playerPause() {
    if (playerCallbacks.indexOf("pause") > -1) {
        executeScript(`
            function() {
                try {
                    ${mediaSessionsClassName}.executeCallback("pause");
                } catch (e) {
                    console.warn("Exception executing 'pause' media sessions callback", e);
                }
            }
        `);
    } else if (activePlayer) {
        activePlayer.pause();
    }
}

document.addEventListener("DOMContentLoaded", function() {

    registerAllPlayers();

    // TODO figure out somehow when a <video> tag is added dynamically and autoplays
    // as can happen on Ajax-heavy pages like YouTube
    // could also be done if we just look for the "audio playing in this tab" and only then check for player?
    // cf. "checkPlayer" event above

    var observer = new MutationObserver(function (mutations) {
        mutations.forEach(function (mutation) {
            mutation.addedNodes.forEach(function (node) {
                if (typeof node.matches !== "function" || typeof node.querySelectorAll !== "function") {
                    return;
                }

                // first check whether the node itself is audio/video
                if (node.matches("video,audio")) {
                    registerPlayer(node);
                    return;
                }

                // if not, check whether any of its children are
                var players = node.querySelectorAll("video,audio");
                players.forEach(function (player) {
                    registerPlayer(player);
                });
            });
        });
    });

    observer.observe(document.documentElement, {
        childList: true,
        subtree: true
    });

    // Observe changes to the <title> tag in case it is updated after the player has started playing
    var titleTag = document.querySelector("head > title");
    if (titleTag) {
        var titleObserver = new MutationObserver(function (mutations) {
            mutations.forEach(function (mutation) {
                var pageTitle = mutation.target.textContent;
                if (pageTitle) {
                    sendMessage("mpris", "titlechange", {
                        pageTitle: pageTitle
                    });
                }
            });
        });

        titleObserver.observe(titleTag, {
            childList: true, // text content is technically a child node
            subtree: true,
            characterData: true
        });
    }

    window.addEventListener("beforeunload", function () {
        // about to navigate to a different page, tell our extension that the player will be gone shortly
        // we listen for tab closed in the extension but we don't for navigating away as URL change doesn't
        // neccesarily mean a navigation but beforeunload *should* be the thing we want

        activePlayer = undefined;
        playerMetadata = {};
        playerCallbacks = [];
        sendMessage("mpris", "gone");
    });

});

// This adds a shim for the Chrome media sessions API which is currently only supported on Android
// Documentation: https://developers.google.com/web/updates/2017/02/media-session
// Try it here: https://googlechrome.github.io/samples/media-session/video.html
//
// TODO Forward mpris calls to the actionHandlers on the page
// previoustrack, nexttrack, seekbackward, seekforward, play, pause

// Bug 379087: Only inject this stuff if we're a proper HTML page
// otherwise we might end up messing up XML stuff
// only if our documentElement is a "html" tag we'll do it
// the rest is only set up in DOMContentLoaded which is only executed for proper pages anyway

// tagName always returned "HTML" for me but I wouldn't trust it always being uppercase
if (document.documentElement.tagName.toLowerCase() === "html") {
    executeScript(`
        function() {
            ${mediaSessionsClassName} = function() {};
            ${mediaSessionsClassName}.callbacks = {};
            ${mediaSessionsClassName}.metadata = {};
            ${mediaSessionsClassName}.playbackState = "none";
            ${mediaSessionsClassName}.sendMessage = function(action, payload) {
                var transferItem = document.getElementById('${mediaSessionsTransferDivId}');
                transferItem.innerText = JSON.stringify({action: action, payload: payload});

                var event = document.createEvent('CustomEvent');
                event.initEvent('payloadChanged', true, true);
                transferItem.dispatchEvent(event);
            };
            ${mediaSessionsClassName}.executeCallback = function (action) {
                this.callbacks[action]();
            };

            navigator.mediaSession = {};
            navigator.mediaSession.setActionHandler = function (name, cb) {
                if (cb) {
                    ${mediaSessionsClassName}.callbacks[name] = cb;
                } else {
                    delete ${mediaSessionsClassName}.callbacks[name];
                }
                ${mediaSessionsClassName}.sendMessage("callbacks", Object.keys(${mediaSessionsClassName}.callbacks));
            };
            Object.defineProperty(navigator.mediaSession, "metadata", {
                get: function() { return ${mediaSessionsClassName}.metadata; },
                set: function(newValue) {
                    ${mediaSessionsClassName}.metadata = newValue;
                    ${mediaSessionsClassName}.sendMessage("metadata", newValue.data);
                }
            });
            Object.defineProperty(navigator.mediaSession, "playbackState", {
                get: function() { return ${mediaSessionsClassName}.playbackState; },
                set: function(newValue) {
                    ${mediaSessionsClassName}.playbackState = newValue;
                    ${mediaSessionsClassName}.sendMessage("playbackState", newValue);
                }
            });

            window.MediaMetadata = function (data) {
                this.data = data;
            };
        }
    `);

    // here we replace the document.createElement function with our own so we can detect
    // when an <audio> tag is created that is not added to the DOM which most pages do
    // while a <video> tag typically ends up being displayed to the user, audio is not.
    // HACK We cannot really pass variables from the page's scope to our content-script's scope
    // so we just blatantly insert the <audio> tag in the DOM and pick it up through our regular
    // mechanism. Let's see how this goes :D

    executeScript(`function() {
            var oldCreateElement = document.createElement;
            document.createElement = function () {
                var createdTag = oldCreateElement.apply(this, arguments);

                var tagName = arguments[0];

                if (typeof tagName === "string" && tagName.toLowerCase() === "audio") {
                    (document.head || document.documentElement).appendChild(createdTag);
                }

                return createdTag;
            };
        }
    `);

    // now the fun part of getting the stuff from our page back into our extension...
    // cannot access extensions from innocent page JS for security
    var transferItem = document.createElement("div");
    transferItem.setAttribute("id", mediaSessionsTransferDivId);
    transferItem.style.display = "none";

    (document.head || document.documentElement).appendChild(transferItem);

    transferItem.addEventListener('payloadChanged', function() {
        var json = JSON.parse(this.innerText);

        var action = json.action

        if (action === "metadata") {
            // FIXME filter metadata, this stuff comes from a hostile environment after all

            playerMetadata = json.payload;

            sendMessage("mpris", "metadata", json.payload);
        } else if (action === "playbackState") {
            var playbackState = json.payload;

            if (activePlayer) {
                if (playbackState === "playing") {
                    playerPlaying(activePlayer);
                } else if (playbackState === "paused") {
                    playerPaused(activePlayer);
                }
            }

        } else if (action === "callbacks") {
            playerCallbacks = json.payload;
            sendMessage("mpris", "callbacks", json.payload);
        }
    });
}
