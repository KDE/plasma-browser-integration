/*
    GNOME Shell integration for Chrome
    Copyright (C) 2016  Yuri Konotopov <ykonotopov@gnome.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

m = chrome.i18n.getMessage;
i18n = (function($) {
	return function() {
		$('[data-i18n]').each(function () {
			var data = $.map($(this).data('i18n').split(','), function(value) {
				value = value.trim();

				if(value.startsWith('__MSG_'))
				{
					return value.replace(/__MSG_(\w+)__/g, function(match, key)
					{
					    return key ? m(key) : "";
					});
				}

				return value;
			});

			if(data.length)
			{
				if($(this).data('i18n-html'))
				{
					$(this).html(m(data[0], data.slice(1)));
				}
				else
				{
					$(this).text(m(data[0], data.slice(1)));
				}
			}
		});
	}
})(jQuery);
