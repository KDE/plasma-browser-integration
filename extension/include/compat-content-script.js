/*
    GNOME Shell integration for Chrome
    Copyright (C) 2016  Yuri Konotopov <ykonotopov@gnome.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

if(COMPAT.IS_FIREFOX)
{
	(function() {
		// Define the API subset provided to the webpage
		var externalMessaging = {
			runtime: {
				sendMessage: function (extensionId, message, options, responseCallback) {
					if(extensionId !== chrome.i18n.getMessage('@@extension_id'))
					{
						console.error('Wrong extension id provided.')
						return;
					}

					if(typeof(options) === 'function')
					{
						responseCallback = options;
						options = undefined;
					}

					chrome.runtime.sendMessage(extensionId, message, options)
						.then(result => {
							if(typeof(responseCallback) == 'function')
							{
								responseCallback(cloneInto(result, window));
							}
						})
						.catch(err => {
							console.error("firefox-external-messaging: runtime.sendMessage error", err);
						});
				}
			}
		};

		// Inject the API in the webpage wrapped by this content script
		// (exposed as `chrome.runtime.sendMessage({anyProp: "anyValue"}).then(reply => ..., err => ...)`)
		window.wrappedJSObject.chrome = cloneInto(externalMessaging, window, {
			cloneFunctions: true,
		});
	})();
}
