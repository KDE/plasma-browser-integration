console.log("CONTENT SCRIPT", document);

var activePlayer;

function notifyMpris(action, payload) {
    chrome.extension.sendMessage({
        subsystem: "mpris",
        action: action,
        payload: payload || {}
    });
}

document.addEventListener("DOMContentLoaded", function() {
    console.log("CONTENT LOADED");
    // TODO recognize when a new player arrives
    // TODO could be done if we just look for the "audio playing in this tab" and only then check for player?

    var players = document.querySelectorAll("video");
    players.forEach(function (player) {
        player.addEventListener("play", function () {
            notifyMpris("play");
            activePlayer = player;
        });

        player.addEventListener("pause", function () {
            notifyMpris("pause");
        });

        player.addEventListener("timeupdate", function () {

        });

        player.addEventListener("loadedmetadata", function () {
            notifyMpris("metadata", {

            });
        });
    });

});
