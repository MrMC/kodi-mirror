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

	nationwide.tools = {
		'preloadJavaScript' : function (scripts, callback) {
			if (scripts.length) {
				var script = scripts.shift();
				$.ajax({
					url : script,
					dataType : 'script',
					cache: false, // set to false for debuggin/development
					success: function(data, status) {
						if (status === 'success' || status === 'notmodified') {
							nationwide.tools.preloadJavaScript(scripts, callback);
						}
					}
				});
			} else {
				callback();
			}
		}
	}


	nationwide.app = function() {
		this.load();	
	}
	nationwide.app.prototype = {
		constructor: nationwide.app,

		'load': function() {
			var self = this
				, scripts = [
					"scripts/xbmc.core.js",
					"scripts/xbmc.rpc.js"
				];
		
			if (typeof(JSON) == 'undefined' || typeof(JSON.stringify) == 'undefined') {
				scripts.push("scripts/json2.js");	
			}

			nationwide.tools.preloadJavaScript(scripts, function() {self.init()} );
		},
		'init': function() {
			var self = this;

			// bind player controls if on site
			var $controls = $('[data-type="xbmc-player-controls"]');
			if ($controls.length) {
				nationwide.tools.preloadJavaScript(["scripts/xbmc.player.js"], function() {
					var p = self.getPlayer();
					p.bindControls($controls);
				});
			}

			// check for playlist and bind current player
			var $playlist = $('[data-type="xbmc-playlist"]');
			if ($playlist.length) {
				nationwide.tools.preloadJavaScript(["scripts/xbmc.playlist.js"], function() {
					self.playlist = new xbmc.playlist($playlist, self.getPlayer(), {defaultPlaylist: 'audio'});
				});
			}

			// check for the machine settings page
			var $settings = $('[data-type="nationwide-machinesettings"]');
			if ($settings.length) {
				nationwide.tools.preloadJavaScript(["scripts/nationwide.settings.js"], function() {
					self.settings = new nationwide.settings($settings, {
						addonId:'script.nationwide_helper',
						methodRead:'GetMachineSettings',
						methodWrite:'SaveMachineSettings'
					});
				});
			}
		},
		'getPlayer': function() {
			if (!this.player) {
				this.player = new xbmc.player(null, {
					'onUpdate' : function(e, activePlayer, $controls, player) {
						var $playPause = player.getControl('playPause')
							, playMode = $playPause.data('playingMode');
						if (activePlayer) {
							if (activePlayer.isPlaying && !playMode) {
								$playPause.html('<i class="icon-pause"></i> Pause').data('playingMode', true);
							} else if (activePlayer.isPaused && playMode) {
								$playPause.html('<i class="icon-play"></i> Play').data('playingMode', false);
							}
						} else if (playMode){
							$playPause.html('<i class="icon-play"></i> Play').data('playingMode', false);
						}
					}
				});
			}
			return this.player;
		}
	};

	window.nationwide = nationwide;

	var app = new nationwide.app();

}(window, jQuery));