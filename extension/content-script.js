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
    chrome.extension.sendMessage({
        subsystem: subsystem,
        action: action,
        payload: payload
    });
}

function executeScript(script) {
    var element = document.createElement('script');
    element.innerHTML = '('+ script +')();';
    (document.body || document.head || document.documentElement).appendChild(element);
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

// MPRIS
// ------------------------------------------------------------------------
//
var activePlayer;
var playerMetadata = {};
var playerCallbacks = [];

var players = [];

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
        executeScript("plasmaMediaSessions.executeCallback('nexttrack')");
    }
});

addCallback("mpris", "previous", function () {
    if (playerCallbacks.indexOf("previoustrack") > -1) {
        executeScript("plasmaMediaSessions.executeCallback('previoustrack')");
    }
});

addCallback("mpris", "setPosition", function (message) {
    if (activePlayer) {
        activePlayer.currentTime = message.position;
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

// TODO this thing will eventually be invoked by our extension to ask the page
// for a player. We could potentially hook that up to the "playing audio" icon on the tab
// or check that when new metadata arrives over media sessions or something like that
addCallback("mpris", "checkPlayer", function () {
    //registerAllPlayers();
});

function setPlayerActive(player) {
    activePlayer = player;
    // when playback starts, send along metadata
    // a website might have set Media Sessions metadata prior to playing
    // and then we would have ignored the metadata signal because there was no player
    sendMessage("mpris", "playing", {
        duration: player.duration,
        currentTime: player.currentTime,
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

    console.log("Register player", player);

    // auto-playing player, become active right away
    if (!player.paused) {
        setPlayerActive(player);
    }
    player.addEventListener("play", function () {
        setPlayerActive(player);
    });

    player.addEventListener("pause", function () {
        sendPlayerInfo(player, "paused");
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

    // TODO use player.seekable for determining whether we can seek?
    player.addEventListener("durationchange", function () {
        sendPlayerInfo(player, "duration", {
            duration: player.duration
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
        executeScript("plasmaMediaSessions.executeCallback('play')");
    } else if (activePlayer) {
        activePlayer.play();
    }
}

function playerPause() {
    if (playerCallbacks.indexOf("pause") > -1) {
        executeScript("plasmaMediaSessions.executeCallback('pause')");
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
                if (node.tagName === "VIDEO") {
                    registerPlayer(node);
                } else {
                    registerAllPlayers(); // FIXME omg this is horrible, doing that every single time the dom changes
                }
            });
        });
    });

    observer.observe(document.body, {
        childList: true,
        subtree: true
    });

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

var scriptTag = document.createElement("script");
scriptTag.innerHTML = `
    plasmaMediaSessions = function() {};
    plasmaMediaSessions.callbacks = {};
    plasmaMediaSessions.metadata = {};
    plasmaMediaSessions.playbackState = "none";
    plasmaMediaSessions.sendMessage = function(action, payload) {
        var transferItem = document.getElementById('plasma-browser-integration-media-session-transfer');
        transferItem.innerText = JSON.stringify({action: action, payload: payload});

        var event = document.createEvent('CustomEvent');
        event.initEvent('payloadChanged', true, true);
        transferItem.dispatchEvent(event);
    };
    plasmaMediaSessions.executeCallback = function (action) {
        this.callbacks[action]();
    };

    navigator.mediaSession = {};
    navigator.mediaSession.setActionHandler = function (name, cb) {
        if (cb) {
            plasmaMediaSessions.callbacks[name] = cb;
        } else {
            delete plasmaMediaSessions.callbacks[name];
        }
        plasmaMediaSessions.sendMessage("callbacks", Object.keys(plasmaMediaSessions.callbacks));
    };
    Object.defineProperty(navigator.mediaSession, "metadata", {
        get: function() { return plasmaMediaSessions.metadata; },
        set: function(newValue) {
            plasmaMediaSessions.metadata = newValue;
            plasmaMediaSessions.sendMessage("metadata", newValue.data);
        }
    });
    Object.defineProperty(navigator.mediaSession, "playbackState", {
        get: function() { return plasmaMediaSessions.playbackState; },
        set: function(newValue) {
            plasmaMediaSessions.playbackState = newValue;
            plasmaMediaSessions.sendMessage("playbackState", newValue);
        }
    });

    window.MediaMetadata = function (data) {
        this.data = data;
    };
`;

(document.head || document.documentElement).appendChild(scriptTag);

// now the fun part of getting the stuff from our page back into our extension...
// cannot access extensions from innocent page JS for security
var transferItem = document.createElement("div");
transferItem.setAttribute("id", "plasma-browser-integration-media-session-transfer");
transferItem.style.display = "none";

(document.head || document.documentElement).appendChild(transferItem);

transferItem.addEventListener('payloadChanged', function() {
    var json = JSON.parse(this.innerText);

    var action = json.action

    if (action === "metadata") {
        // FIXME filter metadata, this stuff comes from a hostile environment after all

        playerMetadata = json.payload;

        sendMessage("mpris", "metadata", json.payload);
    /*} else if (action === "playbackState") {
        playerPlaybackState = json.payload;
        sendMessage("mpris", "playbackState", json.payload);*/
    } else if (action === "callbacks") {
        playerCallbacks = json.payload;
        sendMessage("mpris", "callbacks", json.payload);
    }
});
