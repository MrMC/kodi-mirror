/*
 *      Copyright (C) 2005-2012 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

(function (window, $) {

    var xbmc = window.xbmc || {};

    xbmc.rpc = {
        'default_options': {
            contentType: 'application/json; charset=UTF-8',
            dataType: 'json',
            type: 'POST',
            success: function () {
                xbmc.core.hideBusyDialog();
            }
        },
        'request': function (options) {
            var request_options = $.extend({}, this.default_options, options)
					, url = xbmc.core.JSON_RPC + '?' + options.method;
            request_options.data = JSON.stringify({
                'jsonrpc': '2.0',
                'method': options.method,
                'id': 1,
                'params': request_options.params
            });
				// don't set the "method" property in here as it will override "make the json
				delete(request_options.method)
				delete(request_options.params)

            return $.ajax(url, request_options)
        }
    };

    window.xbmc = xbmc;

}(window, jQuery));

