/*
    GNOME Shell integration for Chrome
    Copyright (C) 2016  Yuri Konotopov <ykonotopov@gnome.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

GSC.update = (function($) {
	function schedule(updateCheckPeriod, skipCheck) {
		if(!skipCheck)
		{
			check();
		}

		chrome.alarms.create(
			ALARM_UPDATE_CHECK,
			{
				delayInMinutes: updateCheckPeriod * 60,
				periodInMinutes: updateCheckPeriod * 60
			}
		);

		chrome.runtime.sendMessage(GS_CHROME_ID, MESSAGE_NEXT_UPDATE_CHANGED);
	}

	function check() {
		GSC.onInitialize().then(response => {
			if (response.success)
			{
				var shellVersion = response.properties.shellVersion;

				// TODO: remove deprecated in version 9
				if(GSC.nativeUpdateCheckSupported(response))
				{
					GSC.sendNativeRequest({execute: 'checkUpdate', url: UPDATE_URL}, function (response) {
						if (response.success)
						{
							onSweetToothResponse(response.upgrade, response.extensions);
						}
						else
						{
							createUpdateFailedNotification(response.message ? response.message : m('native_request_failed', 'checkUpdate'));
						}
					});
				}
				else
				{
					_frontendCheck(shellVersion);
				}
			}
			else
			{
				createUpdateFailedNotification(response.message ? response.message : m('native_request_failed', 'initialize'));
			}
		});
	}

	/*
	 * TODO: remove in version 9
	 * @Deprecated
	 */
	function _frontendCheck(shellVersion)
	{
		GSC.sendNativeRequest({execute: 'listExtensions'}, function (extensionsResponse) {
			if (extensionsResponse.success)
			{
				if ($.isEmptyObject(extensionsResponse.extensions))
					return;

				var request = {
					shell_version: shellVersion,
					installed: {}
				};

				for (uuid in extensionsResponse.extensions)
				{
					if (GSC.isUUID(uuid) && extensionsResponse.extensions[uuid].type == EXTENSION_TYPE.PER_USER)
					{
						request.installed[uuid] = {version: parseInt(extensionsResponse.extensions[uuid].version) || 1};
					}
				}

				request.installed = JSON.stringify(request.installed);

				chrome.permissions.contains({
					permissions: ["webRequest"]
				}, function (webRequestEnabled) {
					if (webRequestEnabled)
					{
						chrome.webRequest.onErrorOccurred.addListener(
								onNetworkError,
								{
									urls: [UPDATE_URL + "*"],
									types: ['xmlhttprequest']
								}
						);
					}

					$.ajax({
						url: UPDATE_URL,
						data: request,
						dataType: 'json',
						method: 'GET',
						cache: false
					})
						.done(function(data) {
							onSweetToothResponse(data, extensionsResponse.extensions)
						})
						.fail(function (jqXHR, textStatus, errorThrown) {
							if (textStatus === 'error' && !errorThrown)
							{
								if (webRequestEnabled)
								{
									return;
								}

								textStatus = m('network_error');
							}

							createUpdateFailedNotification(textStatus);
						}).always(function () {
							if (webRequestEnabled)
							{
								chrome.webRequest.onErrorOccurred.removeListener(onNetworkError);
							}
						});
				});
			} else
			{
				createUpdateFailedNotification(response.message ? response.message : m('native_request_failed', 'listExtensions'));
			}
		});
	}

	function onSweetToothResponse(data, installedExtensions) {
		GSC.notifications.remove(NOTIFICATION_UPDATE_CHECK_FAILED);

		var toUpgrade = [];
		for (uuid in data)
		{
			if (installedExtensions[uuid] && $.inArray(data[uuid], ['upgrade', 'downgrade']) !== -1)
			{
				toUpgrade.push({
					title: installedExtensions[uuid].name,
					message: m('extension_status_' + data[uuid])
				});
			}
		}

		if (toUpgrade.length > 0)
		{
			GSC.notifications.create(NOTIFICATION_UPDATE_AVAILABLE, {
				type: chrome.notifications.TemplateType.LIST,
				title: m('update_available'),
				message: '',
				items: toUpgrade
			});
		}

		chrome.storage.local.set({
			lastUpdateCheck: new Date().toLocaleString()
		});
	}

	function createUpdateFailedNotification(cause) {
		GSC.notifications.create(NOTIFICATION_UPDATE_CHECK_FAILED, {
			message: m('update_check_failed', cause),
			buttons: [
				{title: m('retry')},
				{title: m('close')}
			]
		});
	}

	function onNetworkError(details) {
		createUpdateFailedNotification(details.error);
	}

	function init() {
		function onNotificationAction(notificationId, buttonIndex) {
			if ($.inArray(notificationId, [NOTIFICATION_UPDATE_AVAILABLE, NOTIFICATION_UPDATE_CHECK_FAILED]) === -1)
				return;

			if (notificationId === NOTIFICATION_UPDATE_CHECK_FAILED && buttonIndex == 0)
			{
				check();
			}

			GSC.notifications.remove(notificationId);
		}

		function onNotificationClicked(notificationId) {
			if (notificationId === NOTIFICATION_UPDATE_AVAILABLE)
			{
				chrome.tabs.create({
					url: EXTENSIONS_WEBSITE + 'local/',
					active: true
				});
			}
		}

		chrome.alarms.onAlarm.addListener(function (alarm) {
			if (alarm.name === ALARM_UPDATE_CHECK)
			{
				check();

				chrome.alarms.get(ALARM_UPDATE_CHECK, function (alarm) {
					if (alarm && alarm.periodInMinutes && ((alarm.scheduledTime - Date.now()) / 1000 / 60 < alarm.periodInMinutes * 0.9))
					{
						schedule(alarm.periodInMinutes / 60, true);
					}
					else
					{
						chrome.runtime.sendMessage(GS_CHROME_ID, MESSAGE_NEXT_UPDATE_CHANGED);
					}
				});
			}
		});

		GSC.onInitialize().then(response => {
			/*
				@Deprecated: remove browser notifications in version 9
			 */
			if (!GSC.nativeNotificationsSupported(response))
			{
				chrome.notifications.onClicked.addListener(function (notificationId) {
					onNotificationClicked(notificationId);
				});

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
							else if(request.signal == SIGNAL_NOTIFICATION_CLICKED)
							{
								onNotificationClicked(request.name);
							}
						}
					}
				);
			}
		});

		chrome.storage.onChanged.addListener(function (changes, areaName) {
			if (changes.updateCheck)
			{
				if (!changes.updateCheck.newValue)
				{
					chrome.alarms.clear(ALARM_UPDATE_CHECK);
				}
				else
				{
					chrome.storage.sync.get(DEFAULT_SYNC_OPTIONS, function (options) {
						schedule(options.updateCheckPeriod);
					});
				}
			}
			else if (changes.updateCheckPeriod)
			{
				chrome.storage.sync.get(DEFAULT_SYNC_OPTIONS, function (options) {
					if (options.updateCheck)
					{
						schedule(options.updateCheckPeriod);
					}
				});
			}
		});

		chrome.storage.sync.get(DEFAULT_SYNC_OPTIONS, function (options) {
			if (options.updateCheck)
			{
				chrome.alarms.get(ALARM_UPDATE_CHECK, function (alarm) {
					if (!alarm || !alarm.periodInMinutes || alarm.periodInMinutes !== options.updateCheckPeriod * 60)
					{
						schedule(options.updateCheckPeriod);
					}
				});
			}
		});
	}

	return {
		init: init,
		check: check,
		schedule: schedule
	};
})(jQuery);
