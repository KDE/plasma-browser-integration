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

var activePlayer;

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
addCallback("mpris", "play", function () {
    if (activePlayer) {
        activePlayer.play();
    }
});

addCallback("mpris", "pause", function () {
    if (activePlayer) {
        activePlayer.pause();
    }
});

addCallback("mpris", "playPause", function () {
    if (activePlayer) {
        if (activePlayer.paused) {
            activePlayer.play();
        } else {
            activePlayer.pause();
        }
    }
});

function setPlayerActive(player) {
    activePlayer = player;
    sendMessage("mpris", "playing");
}

function sendPlayerInfo(player, event, payload) {
    if (player != activePlayer) {
        return;
    }

    sendMessage("mpris", event, payload);
}

document.addEventListener("DOMContentLoaded", function() {
    // TODO recognize when a new player arrives
    // TODO could be done if we just look for the "audio playing in this tab" and only then check for player?

    var players = document.querySelectorAll("video");
    players.forEach(function (player) {
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

        // TODO use "rate" instead
        /*player.addEventListener("timeupdate", function () {

        });*/

        // TODO use player.seekable for determining whether we can seek?
        sendPlayerInfo(player, "duration", {
            duration: player.duration
        });
        player.addEventListener("durationchange", function () {
            sendPlayerInfo(player, "duration", {
                duration: player.duration
            });
        });

        player.addEventListener("seeked", function () {
            // TODO send pos along so we can update the mpris Position
        });
    });

});
