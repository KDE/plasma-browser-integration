/*
    GNOME Shell integration for Chrome
    Copyright (C) 2016  Yuri Konotopov <ykonotopov@gnome.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

/*
 * Main object that handles extensions synchronization with remote storage.
 */
GSC.sync = (function($) {
	/*
	 * Initialization rutines.
	 */
	function init() {
		if(!COMPAT.SYNC_STORAGE)
		{
			return;
		}

		function onNotificationAction(notificationId, buttonIndex) {
			if (notificationId !== NOTIFICATION_SYNC_FAILED)
			{
				return;
			}

			GSC.notifications.remove(notificationId);
		}

		onSyncFromRemote();
		chrome.storage.onChanged.addListener(function(changes, areaName) {
			if(areaName === 'sync' && changes.extensions)
			{
				onSyncFromRemote(changes.extensions.newValue);
			}
		});

		chrome.runtime.onMessage.addListener(
			function (request, sender, sendResponse) {
				if (sender.id && sender.id === GS_CHROME_ID && request)
				{
					if (request === MESSAGE_SYNC_FROM_REMOTE)
					{
						onSyncFromRemote();
					}
				}
			}
		);

		GSC.onInitialize().then(response => {
			/*
				@Deprecated: remove browser notifications in version 9
			 */
			if (!GSC.nativeNotificationsSupported(response))
			{
				chrome.notifications.onButtonClicked.addListener(onNotificationAction);
			}
			else
			{
				chrome.runtime.onMessage.addListener(
					function (request, sender, sendResponse) {
						if(
							sender.id && sender.id === GS_CHROME_ID &&
							request && request.signal)
						{
							if(request.signal == SIGNAL_NOTIFICATION_ACTION)
							{
								onNotificationAction(request.name, request.button_id);
							}
						}
					}
				);
			}
		});
	}

	/*
	 * Returns array of all local and remote extensions with structure:
	 * [
	 *	$extension_uuid: {
	 *		uuid:		extension uuid,
	 *		name:		extension name,
	 *		remoteState:	extension state in remote storage,
	 *		localState:	extension state in current GNOME Shell,
	 *		remote:		true if extensions is in remote storage,
	 *		local:		true if extension installed localy
	 *	},
	 *	...
	 * ]
	 */
	function getExtensions(deferred, remoteExtensions) {
		GSC.sendNativeRequest({
			execute: 'listExtensions'
		}, function(response) {
			if(response && response.success)
			{
				if(remoteExtensions)
				{
					deferred.resolve(mergeExtensions(remoteExtensions, response.extensions));
				}
				else
				{
					chrome.storage.sync.get({
						extensions: {}
					}, function(options) {
						if(chrome.runtime.lastError)
						{
							deferred.reject(chrome.runtime.lastError.message);
						}
						else
						{
							var extensions = mergeExtensions(options.extensions, response.extensions);
							deferred.resolve(extensions);
						}
					});
				}
			}
			else
			{
				var message = response && response.message ? response.message : m('error_extension_response');
				deferred.reject(message);
			}
		});
	}

	/*
	 * Returns merged list of extensions list in remote storage and
	 * locally installed extensions.
	 * 
	 * Both parameters should be in form:
	 * {
	 *	$extension_uuid: {
	 *		uuid:	,
	 *		name:	,
	 *		state:	
	 *	},
	 *	...
	 * }
	 */
	function mergeExtensions(remoteExtensions, localExtensions)
	{
		var extensions = {};

		$.each(remoteExtensions, function(key, extension) {
			if(extension.uuid && extension.name && extension.state)
			{
				extensions[extension.uuid] = {
					uuid:		extension.uuid,
					name:		extension.name,
					remoteState:	extension.state,
					remote:		true,
					local:		false
				};
			}
		});

		$.each(localExtensions, function(key, extension) {
			if(extensions[extension.uuid])
			{
				extensions[extension.uuid].name = extension.name;
				extensions[extension.uuid].localState = extension.state;
				extensions[extension.uuid].local = true;
			}
			else
			{
				extensions[extension.uuid] = {
					uuid:		extension.uuid,
					name:		extension.name,
					remoteState:	EXTENSION_STATE.UNINSTALLED,
					localState:	extension.state,
					remote:		false,
					local:		true
				};
			}
		});

		return extensions;
	}

	/*
	 * Synchronize local changed extensions to remote list.
	 * 
	 * @param extension - extension object:
	 * {
	 *	uuid:	extension uuid,
	 *	name:	extension name,
	 *	state:	extension state
	 * }
	 */
	function localExtensionChanged(extension) {
		if($.inArray(extension.state, [EXTENSION_STATE.ENABLED, EXTENSION_STATE.DISABLED, EXTENSION_STATE.UNINSTALLED]) !== -1)
		{
			chrome.storage.sync.get({
				extensions: {}
			}, function (options) {
				GSC.sendNativeRequest({
					execute:	'getExtensionInfo',
					uuid:		extension.uuid
				}, function(response) {
					// Extension can be uninstalled already
					if(response && response.extensionInfo && !$.isEmptyObject(response.extensionInfo))
					{
						extension = response.extensionInfo;
					}

					if(extension.state === EXTENSION_STATE.UNINSTALLED && options.extensions[extension.uuid])
					{
						delete options.extensions[extension.uuid];
					}
					else
					{
						options.extensions[extension.uuid] = {
							uuid:	extension.uuid,
							name:	extension.name,
							state:	extension.state
						};
					}

					chrome.storage.sync.set({
						extensions: options.extensions
					});
				});
			});
		}
	}

	/*
	 * Synchronize remote changes with local GNOME Shell.
	 * 
	 * @param remoteExtensions - (optional) remote extensions list
	 */
	function remoteExtensionsChanged(remoteExtensions) {
		getExtensions($.Deferred().done(function(extensions) {
			var enableExtensions = [];
			$.each(extensions, function(uuid, extension) {
				if(extension.remote)
				{
					if(!extension.local)
					{
						GSC.sendNativeRequest({
							execute: "installExtension",
							uuid: extension.uuid
						}, onInstallUninstall);
					}
					else if (extension.remoteState !== extension.localState)
					{
						if(extension.remoteState === EXTENSION_STATE.ENABLED)
						{
							enableExtensions.push({
								uuid: extension.uuid,
								enable: true
							});
						}
						else
						{
							enableExtensions.push({
								uuid: extension.uuid,
								enable: false
							});
						}
					}
				}
				else if(extension.local)
				{
					GSC.sendNativeRequest({
						execute: "uninstallExtension",
						uuid: extension.uuid
					}, onInstallUninstall);
				}
			});

			if(enableExtensions.length > 0)
			{
				GSC.sendNativeRequest({
					execute: "enableExtension",
					extensions: enableExtensions
				});
			}
		}).fail(function(message) {
			createSyncFailedNotification(message);
		}), remoteExtensions);
	}

	/*
	 * Callback called when extension is installed or uninstalled as part
	 * of synchronization process.
	 */
	function onInstallUninstall(response) {
		if(response)
		{
			if(!response.success)
			{
				createSyncFailedNotification(response.message);
			}
		}
		else
		{
			createSyncFailedNotification();
		}
	}

	/*
	 * Wrapper for localExtensionChanged that checks if synchronization is
	 * enabled.
	 */
	function onExtensionChanged(request)
	{
		if(!COMPAT.SYNC_STORAGE)
		{
			return;
		}

		runIfSyncEnabled(function() {
			localExtensionChanged({
				uuid:	request.parameters[EXTENSION_CHANGED_UUID],
				state:	request.parameters[EXTENSION_CHANGED_STATE],
				error:	request.parameters[EXTENSION_CHANGED_ERROR]
			});
		});
	}

	/*
	 * Wrapper for remoteExtensionsChanged that checks if synchronization is
	 * enabled.
	 */
	function onSyncFromRemote(remoteExtensions)
	{
		if(!COMPAT.SYNC_STORAGE)
		{
			return;
		}

		runIfSyncEnabled(function() {
			remoteExtensionsChanged(remoteExtensions);
		});
	}

	/*
	 * Runs callback function if synchronyzation is enabled.
	 * 
	 * @param callback - callback function
	 */
	function runIfSyncEnabled(callback) {
		chrome.storage.local.get({
			syncExtensions: false
		}, function (options) {
			if (options.syncExtensions)
			{
				callback();
			}
		});
	}

	/*
	 * Create notification when synchronization failed.
	 */
	function createSyncFailedNotification(cause) {
		GSC.notifications.create(NOTIFICATION_SYNC_FAILED, {
			message: m('synchronization_failed', cause ? cause : m('unknown_error'))
		});
	}

	/*
	 * Public methods.
	 */
	return {
		init: init,
		getExtensions: getExtensions,
		onExtensionChanged: onExtensionChanged
	};
})(jQuery);
