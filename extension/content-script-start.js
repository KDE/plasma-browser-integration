/*
    KDE Shell integration for Chrome
    Copyright (C) 2016  Yuri Konotopov <ykonotopov@gnome.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

/*
 * Hijack require property to disallow execution of main extensions.gnome.org
 * script until extension initializated.
 */

console.log("HAAAAAAAAALLO");

/*var gs_require_inject = function () {
	GS_CHROME_ID		= "${GS_CHROME_ID}";
	GS_CHROME_VERSION	= "${GS_CHROME_VERSION}";
};

var siteMessages = {};
if(EXTERNAL_MESSAGES)
{
	for(var key in EXTERNAL_MESSAGES)
	{
		siteMessages[EXTERNAL_MESSAGES[key]] = chrome.i18n.getMessage(EXTERNAL_MESSAGES[key]);
	}
}

var s = document.createElement('script');

s.type = "text/javascript";
s.textContent = '(' +
	gs_require_inject.toString()
		.replace("${GS_CHROME_ID}", GS_CHROME_ID)
		.replace("${GS_CHROME_VERSION}", chrome.runtime.getManifest().version)
	+ ")(); GSC = {i18n: JSON.parse('" + JSON.stringify(siteMessages).replace(/'/g, "\\'") + "')};";
(document.head||document.documentElement).appendChild(s);
s.parentNode.removeChild(s);

chrome.runtime.onMessage.addListener(
	function (request, sender, sendResponse) {
		if(
			sender.id && sender.id === GS_CHROME_ID &&
			request && request.signal && [SIGNAL_EXTENSION_CHANGED, SIGNAL_SHELL_APPEARED].indexOf(request.signal) !== -1)
		{
			window.postMessage(
				{
					type: "gs-chrome",
					request: request
				}, "*"
			);
		}
	}
);

s = document.createElement('script');

s.src = chrome.extension.getURL('include/sweettooth-api.js');
s.onload = function() {
    this.parentNode.removeChild(this);
};
(document.head || document.documentElement).appendChild(s);
*/
