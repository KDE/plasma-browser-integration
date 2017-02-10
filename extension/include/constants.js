/*
    GNOME Shell integration for Chrome
    Copyright (C) 2016  Yuri Konotopov <ykonotopov@gnome.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

GS_CHROME_ID				= chrome.i18n.getMessage('@@extension_id');
PLATFORMS_WHITELIST			= ["linux", "openbsd"];

COMPAT = {
	IS_FIREFOX:	 (typeof(InstallTrigger) !== 'undefined'),
	IS_OPERA:	 navigator.userAgent.indexOf(' OPR/') >= 0
};

NOTIFICATION_SYNC_FAILED		= 'gs-chrome-sync-fail';
NOTIFICATION_UPDATE_AVAILABLE		= 'gs-chrome-update';
NOTIFICATION_UPDATE_CHECK_FAILED	= 'gs-chrome-update-fail';
ALARM_UPDATE_CHECK			= 'gs-chrome-update-check';

MESSAGE_NEXT_UPDATE_CHANGED		= 'gs-next-update-changed';
MESSAGE_SYNC_FROM_REMOTE		= 'gs-sync-from-remote';

SIGNAL_EXTENSION_CHANGED		= 'ExtensionStatusChanged';
SIGNAL_NOTIFICATION_ACTION		= 'NotificationAction';
SIGNAL_NOTIFICATION_CLICKED		= 'NotificationClicked';
SIGNAL_SHELL_APPEARED			= 'org.gnome.Shell';

EXTENSION_CHANGED_UUID			= 0;
EXTENSION_CHANGED_STATE			= 1;
EXTENSION_CHANGED_ERROR			= 2;

NATIVE_HOST				= 'org.gnome.chrome_gnome_shell';

EXTENSIONS_WEBSITE			= 'https://extensions.gnome.org/';
UPDATE_URL				= EXTENSIONS_WEBSITE + 'update-info/';

DEFAULT_SYNC_OPTIONS			= {
	showReleaseNotes:	true,
	updateCheck:		true,
	updateCheckPeriod:	6
};

DEFAULT_LOCAL_OPTIONS			= {
	syncExtensions:		false
};

EXTERNAL_MESSAGES			= [
	"connecting_host_app",
	"error_extension_response",
	"no_host_connector",
	"warning_apis_missing"
];

// gnome-shell/js/ui/extensionSystem.js
EXTENSION_STATE			= {
	ENABLED:	1,
	DISABLED:	2,
	ERROR:		3,
	OUT_OF_DATE:	4,
	DOWNLOADING:	5,
	INITIALIZED:	6,

	// Used as an error state for operations on unknown extensions,
	// should never be in a real extensionMeta object.
	UNINSTALLED:	99
};

// gnome-shell/js/misc/extensionUtils.js
EXTENSION_TYPE			= {
    SYSTEM:		1,
    PER_USER:	2
};
