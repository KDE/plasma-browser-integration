(function() {
    let mprisTransferObject = null;
    let eventCallback = function(e) {
        e.stopPropagation();
        const args = e.detail;
        if (args.action == "unload") {
            // TODO: Undo other operations
            window.removeEventListener("org.kde.pbi.event", eventCallback, {"capture": true});
        } else if (args.action == "mediaSessionsRegister") {
            const MediaSessionsClassName_constructor = function() {
                this.callbacks = {};
                this.pendingCallbacksUpdate = 0;
                this.metadata = null;
                this.playbackState = "none";

                this.sendMessage = function(action, payload) {
                    let event = new CustomEvent("org.kde.pbi.mpris.message", {
                        detail: {
                            action: action,
                            payload: payload
                        }
                    });
                    window.dispatchEvent(event);
                };

                this.executeCallback = function(action) {
                    let details = {
                        action: action
                        // for seekforward, seekbackward, seekto there's additional information one would need to add
                    };
                    this.callbacks[action](details);
                };

                this.setCallback = function(name, cb) {
                    const oldCallbacks = Object.keys(this.callbacks).sort();

                    if (cb) {
                        this.callbacks[name] = cb;
                    } else {
                        delete this.callbacks[name];
                    }

                    const newCallbacks = Object.keys(this.callbacks).sort();

                    if (oldCallbacks.toString() === newCallbacks.toString()) {
                        return;
                    }

                    if (this.pendingCallbacksUpdate) {
                        return;
                    }

                    this.pendingCallbacksUpdate = setTimeout(() => {
                        this.pendingCallbacksUpdate = 0;

                        // Make sure to send the current callbacks, not "newCallbacks" at the time of starting the timeout
                        const callbacks = Object.keys(this.callbacks);
                        this.sendMessage("callbacks", callbacks);
                    }, 0);
                };

                this.setMetadata = function(metadata) {
                    // MediaMetadata is not a regular Object so we cannot just JSON.stringify it
                    let newMetadata = {};

                    let dirty = (!metadata != !this.metadata);
                    if (metadata) {
                        const keys = ["title", "artist", "album", "artwork"];

                        const oldMetadata = this.metadata || {};

                        keys.forEach((key) => {
                            const value = metadata[key];

                            if (typeof value?.toString !== "function") {
                                return; // continue
                            }

                            // We only have Strings or the "artwork" Array, so a toString() comparison should suffice...
                            dirty |= (value.toString() !== (oldMetadata[key] || "").toString());

                            newMetadata[key] = value;
                        });
                    }

                    this.metadata = metadata;

                    if (dirty) {
                        this.sendMessage("metadata", newMetadata);
                    }
                };

                this.setPlaybackState = function(playbackState) {
                    if (this.playbackState === playbackState) {
                        return;
                    }

                    this.playbackState = playbackState;
                    this.sendMessage("playbackState", playbackState);
                };
            };

            mprisTransferObject = new MediaSessionsClassName_constructor();

            if (!navigator.mediaSession) {
                navigator.mediaSession = {};
            }

            var noop = function() {};

            var oldSetActionHandler = navigator.mediaSession.setActionHandler || noop;
            navigator.mediaSession.setActionHandler = function(name, cb) {
                mprisTransferObject.setCallback(name, cb);

                // Call the original native implementation
                // "call()" is needed as the real setActionHandler is a class member
                // and calling it directly is illegal as it lacks the context
                // This may throw for unsupported actions but we registered the callback
                // ourselves before
                return oldSetActionHandler.call(navigator.mediaSession, name, cb);
            };

            Object.defineProperty(navigator.mediaSession, "metadata", {
                get: () => mprisTransferObject.metadata,
                set: (newValue) => {
                    mprisTransferObject.setMetadata(newValue);
                }
            });
            Object.defineProperty(navigator.mediaSession, "playbackState", {
                get: () => mprisTransferObject.playbackState,
                set: (newValue) => {
                    mprisTransferObject.setPlaybackState(newValue);
                }
            });

            if (!window.MediaMetadata) {
                window.MediaMetadata = function(data) {
                    Object.assign(this, data);
                };
                window.MediaMetadata.prototype.title = "";
                window.MediaMetadata.prototype.artist = "";
                window.MediaMetadata.prototype.album = "";
                window.MediaMetadata.prototype.artwork = [];
            }

            // here we replace the document.createElement and window.Audio with our own so we can detect
            // <audio> and <video> elements that are never added to the DOM, which many pages do.
            // The content script only finds players through a MutationObserver, so it would miss them.
            //
            // We cannot pass JS objects from the page's scope to the content script's scope, but DOM
            // nodes are shared between both. A MouseEvent's relatedTarget carries the node across,
            // so we hand the player over that way once it starts playing.
            let announceDetachedPlayer = function(player) {
                player.addEventListener("play", () => {
                    // Players in the DOM are already found by the content script's MutationObserver
                    if (player.isConnected) {
                        return;
                    }

                    // MouseEvent is used only for relatedTarget, which can carry a DOM node
                    // to the content script. CustomEvent's detail can't do that.
                    window.dispatchEvent(new MouseEvent("org.kde.pbi.detachedplayer", {
                        relatedTarget: player
                    }));
                });
            };

            const oldCreateElement = Document.prototype.createElement;
            Document.prototype.createElement = function() {
                const createdTag = oldCreateElement.apply(this, arguments);
                const tagName = arguments[0];

                if (typeof tagName === "string") {
                    if (tagName.toLowerCase() === "audio" || tagName.toLowerCase() === "video") {
                        const player = createdTag;
                        announceDetachedPlayer(player);
                    }
                }
                return createdTag;
            };

            // We also briefly add items created as new Audio() to the DOM so we can control it
            // similar to the document.createElement hack above since we cannot share variables
            // between the actual website and the background script despite them sharing the same DOM
            var oldAudio = window.Audio;
            window.Audio = function(...args) {
                const player = new oldAudio(...args);
                announceDetachedPlayer(player);
                return player;
            };
        } else if (args.action == "mpris") {
            try {
                mprisTransferObject.executeCallback(args.mprisCallbackName);
            } catch (e) {
                console.warn("Exception executing '" + args.mprisCallbackName + "' media sessions callback", e);
            }
        } else {
            console.warn("Unknown page script action" + args.action, args);
        }
    };
    window.addEventListener("org.kde.pbi.event", eventCallback, {"capture": true});
    window.dispatchEvent(new CustomEvent("org.kde.pbi.inited"));
}());
