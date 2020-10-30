/*
 *	  Copyright (C) 2005-2013 Team XBMC
 *	  http://www.xbmc.org
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
	"use strict";

	var nationwide = window.nationwide || {};


	nationwide.settings = function(element, options) {
		this.init(element, options);	
	}
	nationwide.settings.prototype = {
		constructor: nationwide.settings,

		'init': function(element, options) {
			var self = this;
			this.$e = $(element);
			this.fire = this.$e.length ? this.$e.add(this) : $(this);
			this.options = $.extend(true, {}, options, this.$e.data());
			this.settings = {};

			this.readSettings(
				function(data, status) {
					if (status == 'success' && data) {
						self.fillForm(data);
					} else {
						alert('Settings could not be read from backend');
					}
				}
			);

			this.$e.submit(function(e) {
				e.preventDefault();
				var data = self.$e.serializeObject();
				self.writeSettings(data);
			});

			
		},

		/**
		 * Fills the form with given settings
		 *
		 * @param onbject	settings	The settings as object/JSON
		 * @return void
		 */
		'fillForm': function(settings) {
			for (var field in settings) {
				this._fillFormField(field, settings[field]);
			}
		},

		'_fillFormField': function(field, value) {
			var field = field || '';
			if (typeof(value) == 'object' || typeof(value) == 'array') {
				for (var propertyName in value) {
					var fieldName = field.length ? field + '[' + propertyName + ']' : propertyName;
					this._fillFormField(fieldName, value[propertyName]);
				}
			} else {
				var $f = this.$e.find('[name="' + field + '"]');
				if ($f.length) {
					if ($f.length == 1) {
						if ($f.attr('type') != 'checkbox' && $f.attr('type') != 'radio') {
							$f.val(value);
						} else {
							if ($f.val() == value) {
								$f.attr('checked', 'checked');
							} else {
								$f.attr('checked', null);	
							}
						}
					} else {
						var values = value.split(',')
							, i;
						for (i in values) {
							$f.filter('[value="' + values[i] + '"]').attr('checked', 'checked');
						}
					}
				}
			}
		},

		/**
		 * Reads the settings
		 *
		 * @param mixed		callback	callback function or object with callback functions (optional)
		 * @return void
		 */
		'readSettings': function(callback) {
			var setting_url;
			$.ajax({
				cache: false,
				async: false,
				type: 'GET',
				url: 'vfs/' + encodeURIComponent('special://MN/settings.txt'),
				success: function(msg){
				setting_url = 'vfs/' + encodeURIComponent('special://MN/settings.txt');
				},
				error: function(jqXHR, textStatus, errorThrown){
				setting_url = 'settings.txt?' + Math.random(10);
				}
			});
			$.ajax({
				cache: false,
				url: setting_url,
				dataType: "json",
				success: function(settings) {
					if (typeof(callback) == 'function') {
						callback(settings, 'success');
					}
			    }
			});
		},

		/**
		 * Reads the settings
		 *
		 * @param object	settings	The settings to write/save
		 * @param mixed		callback	callback function or object with callback functions (optional)
		 * @return void
		 */
		'writeSettings': function(settings, callback) {
			this._sendAddonAction(
				this.options.addonId,
				'ExecuteAddon',
				{
					'params' : {
						'action' : this.options.methodWrite,
						'values' : JSON.stringify(settings).replace(/,/g,';')
					},
					'wait' : true
				},
				callback
			)
		},

		/**
		 * Sends a addon related action to xbmc
		 *
		 * @param string	id			The id of the addon
		 * @param string	action		The action to call
		 * @param object	params		additional params to send (optional)
		 * @param mixed		callback	callback function or object with callback functions (optional)
		 * @return void
		 */
		'_sendAddonAction': function(id, action, params, callback) {
			var addonId = id || this.options.addonId;
			if (addonId) {
				xbmc.rpc.request({
					'method': 'MN.SaveSettings',
					'params': $.extend( {}, params, {
						'addonid': addonId
					}),
					'success': function(data, status) {
						if (typeof(callback) == 'function') return callback(data, status);
						if (typeof(callback) == 'object' && callback.success) callback.success(data, status); 
					},
					'error': function(data, status) {
						if (typeof(callback) == 'function') return callback(data, status);
						if (typeof(callback) == 'object' && callback.error) callback.error(data, status);
					}
				});
			}
		}
	};

	window.nationwide = nationwide;

}(window, jQuery));


/**
 * This little method converts form values to JSON.
 * Copied from http://stackoverflow.com/questions/1184624/convert-form-data-to-js-object-with-jquery#answer-8407771
 */
(function($){
	$.fn.serializeObject = function(){

		var self = this,
			json = {},
			push_counters = {},
			patterns = {
				"validate": /^[a-zA-Z][a-zA-Z0-9_]*(?:\[(?:\d*|[a-zA-Z0-9_]+)\])*$/,
				"key":	  /[a-zA-Z0-9_]+|(?=\[\])/g,
				"push":	 /^$/,
				"fixed":	/^\d+$/,
				"named":	/^[a-zA-Z0-9_]+$/
			};


		this.build = function(base, key, value){
			base[key] = value;
			return base;
		};

		this.push_counter = function(key){
			if(push_counters[key] === undefined){
				push_counters[key] = 0;
			}
			return push_counters[key]++;
		};

		$.each($(this).serializeArray(), function(){

			// skip invalid keys
			if(!patterns.validate.test(this.name)){
				return;
			}

			var k,
				keys = this.name.match(patterns.key),
				merge = this.value,
				reverse_key = this.name;

			while((k = keys.pop()) !== undefined){

				// adjust reverse_key
				reverse_key = reverse_key.replace(new RegExp("\\[" + k + "\\]$"), '');

				// push
				if(k.match(patterns.push)){
					merge = self.build([], self.push_counter(reverse_key), merge);
				}

				// fixed
				else if(k.match(patterns.fixed)){
					merge = self.build([], k, merge);
				}

				// named
				else if(k.match(patterns.named)){
					merge = self.build({}, k, merge);
				}
			}

			json = $.extend(true, json, merge);
		});

		return json;
	};
})(jQuery);