/*
    GNOME Shell integration for Chrome
    Copyright (C) 2016  Yuri Konotopov <ykonotopov@gnome.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

GSC.notifications = (function($) {
	var DEFAULT_NOTIFICATION_OPTIONS = {
		type: chrome.notifications.TemplateType.BASIC,
		iconUrl: 'icons/GnomeLogo-128.png',
		title: m('gs_chrome'),
		buttons: [
			{title: m('close')}
		],
		priority: 2,
		isClickable: true,
		requireInteraction: true
	};

	function remove_list(options) {
		if(options.items)
		{
			var items = [];
			for (k in options.items)
			{
				if (options.items.hasOwnProperty(k))
				{
					items.push(options.items[k].title + ' ' + options.items[k].message);
				}
			}

			if(options.message && items)
			{
				options.message += "\n";
			}

			options.message += items.join("\n");

			options.type = chrome.notifications.TemplateType.BASIC;
			delete options.items;
		}

		return options;
	}

	/*
		@Deprecated: remove browser notifications in version 9
	 */
	var browser = (function() {
		function init() {
			if(COMPAT.ON_STARTUP) {
				chrome.runtime.onStartup.addListener(function() {
					// Do nothing. We just need this callback to restore notifications
				});
			}

			chrome.notifications.onClosed.addListener(function (notificationId, byUser) {
				if (!byUser)
				{
					update(notificationId);
				}
				else
				{
					remove(notificationId);
				}
			});

			chrome.notifications.onClicked.addListener(function (notificationId) {
				GSC.notifications.remove(notificationId);
			});

			restore();
		}

		function create(name, options) {
			chrome.storage.local.get({
				notifications: {}
			}, function (items) {
				var notifications = items.notifications;

				notifications[name] = $.extend(DEFAULT_NOTIFICATION_OPTIONS, options);

				_create(name, notifications[name], function (notificationId) {
					chrome.storage.local.set({
						notifications: notifications
					});
				});
			});
		}

		function _create(name, options, callback)
		{
			if(!COMPAT.NOTIFICATIONS_BUTTONS && options.buttons)
			{
				delete options.buttons;
			}

			if(COMPAT.IS_OPERA)
			{
				if(options.type === chrome.notifications.TemplateType.LIST)
				{
					options = remove_list(options);
				}
			}

			if (callback)
			{
				chrome.notifications.create(name, options, callback);
			}
			else
			{
				chrome.notifications.create(name, options);
			}
		}

		function update(notificationId) {
			chrome.storage.local.get({
				notifications: {}
			}, function (items) {
				var notifications = items.notifications;

				if (notifications[notificationId])
				{
					_create(notificationId, notifications[notificationId]);
				}
			});
		}

		function remove(notificationId) {
			chrome.storage.local.get({
				notifications: {}
			}, function (items) {
				var notifications = items.notifications;

				if (notifications[notificationId])
				{
					delete notifications[notificationId];
					chrome.storage.local.set({
						notifications: notifications
					});
				}

				chrome.notifications.clear(notificationId);
			});
		}

		function restore() {
			chrome.storage.local.get({
				notifications: {}
			}, function (items) {
				var notifications = items.notifications;

				for (notificationId in notifications)
				{
					update(notificationId);
				}
			});
		}

		return {
			create: create,
			remove: remove,
			init: init
		};
	})();

	var native = (function() {
		function create(name, options) {
			options = remove_list(options);

			chrome.runtime.sendMessage({
				execute: 'createNotification',
				name: name,
				options: $.extend(DEFAULT_NOTIFICATION_OPTIONS, options)
			});
		}

		function remove(notificationId) {
			chrome.runtime.sendMessage({
				execute: 'removeNotification',
				name: notificationId
			});
		}

		return {
			create: create,
			remove: remove
		};
	})();

	GSC.onInitialize().then(response => {
		if (!GSC.nativeNotificationsSupported(response))
		{
			browser.init();
		}
	});

	return {
		create: function() {
			GSC.onInitialize().then(response => {
				if(GSC.nativeNotificationsSupported(response))
				{
					native.create.apply(this, arguments);
				}
				else
				{
					browser.create.apply(this, arguments);
				}
			});
		},
		remove: function() {
			GSC.onInitialize().then(response => {
				if(GSC.nativeNotificationsSupported(response))
				{
					native.remove.apply(this, arguments);
				}
				else
				{
					browser.remove.apply(this, arguments);
				}
			});
		}
	};
})(jQuery);
