/*
 *      Copyright (C) 2005-2013 Team XBMC
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

(function (window) {

	var xbmc = window.xbmc || {};

	xbmc.player = function(element, options) {
		this.init(element, options);
	};

	xbmc.player.prototype = {
		constructor: xbmc.player,

		/**
		 * initializes the player object
		 *
		 * @param dom-node	The optional DOM node/jQuery object to bind this player to
		 * @param object		Optional overriding default options
		 * @return void
		 */
		'init': function(element, options) {
			this.$e = $(element)
			this.fire = this.$e.length ? this.$e.add(this) : $(this);
			this.options = $.extend(true, {}, xbmc.defaults.player, options, this.$e.data())
			this.activePlayers = {}
			this.timers = {}

			// callbacks
			var self = this;
			$.each(['onBeforeInit', 'onInit', 'onPlayerChange', 'onActionStop', 'onActionPlay', 'onActionPause', 'onActionNext', 'onActionPrev', 'onUpdate', 'onPlayerStatusUpdated', 'onActivePlayersUpdated'], function(i, name) {
				// configuration
				if ($.isFunction(self.options[name])) { 
					$(self).bind(name, self.options[name]); 
				}
			});

			// onBeforeInit
			var e = $.Event("onBeforeInit");
			this.fire.trigger(e, [this]);

			if (this.$e.length) {
				this.bindControls(this.$e);
			}

			// do initial update
			this._updateGui();
			this._updateActivePlayers();

			// trigger necessary tasks after playerstatus was updated
			$(this).bind('onPlayerStatusUpdated', function(e, status, result, playerData, obj) {
				if (status == 'success') {
					if (self.timers.activePlayers) {
						clearTimeout(self.timers.activePlayers);
						self.timers.activePlayers = null;
					}
					self.timers.playerStatus = setTimeout( function() { self._updatePlayerStatus(); self.timers.playerStatus = null; }, 1000 );
				} else {
					if (!self.timers.activePlayers) self._updateActivePlayers();
				}
				self._updateGui();
			});

			// create timers to keep the gui etc updated
			// @todo	implement web sockets and get rid of the timeouts
			//			also move the update logic to xbmc.rpc and only bind listeners. xbmc.rpc should deal with sockets/timeouts
			$(this).bind('onActivePlayersUpdated', function(e, status, result, obj) {
				// after update is finished queue next update call
				if (self.timers.activePlayers) clearTimeout(self.timers.activePlayers);
				var delay = status == 'success' ? 1000 : 5000;
				self.timers.activePlayers = setTimeout( function() { self._updateActivePlayers(); self.timers.activePlayers = null }, delay );

				// if we have an active player, update it's status
				if (self.getActivePlayer()) {
					if (!self.timers.playerStatus) self._updatePlayerStatus();
				} else if (self.timers.playerStatus) {
					clearTimeout(self.timers.playerStatus);
					self.timers.playerStatus = null;
				}
			});

			// onInit
			var e = $.Event("onInit");
			this.fire.trigger(e, [this]);
		},

		/**
		 * Binds html player controls found within the passed DOM node
		 *
		 * @param dom-node	DOM node/jQuery with player controls
		 * @return void
		 */
		'bindControls': function(element) {
			this.$controls = $(element);
			var self = this;
			for (var type in this.options.selector.controls) {
				var $c = this.getControl(type);
				if ($c.length) {
					$c.bind('click.xbmc', {'type': type}, function(e) { self._triggerButtonPress(e) });
				}
			}
		},

		/**
		 * updates the control states based on the players status
		 * @todo move GUI stuff to it's own class
		 *
		 * @return void
		 */
		'_updateGui': function() {
			var p = this.getActivePlayer();
			if (p) {
				this.enableControl('stop');
				if (p.isPlaying) {
					this.enableControl('pause');
					this.disableControl('play');
					this.enableControl('playPause')
						.addClass(this.options.classes.controls.playPausePlaying)
						.removeClass(this.options.classes.controls.playPausePaused)
				} else if (p.isPaused) {
					this.enableControl('play');
					this.disableControl('pause');
					this.enableControl('playPause')
						.removeClass(this.options.classes.controls.playPausePlaying)
						.addClass(this.options.classes.controls.playPausePaused)
				}
			} else {
				// no player, disable controls
				for (var type in this.options.selector.controls) {
					this.disableControl(type);
				}
			}

			// fire event
			var e = $.Event("onUpdate");
			this.fire.trigger(e, [p, this.$controls, this]);
		},

		/**
		 * Generic function to execute/trigger the action assigned to player control buttons
		 *
		 * @param e	Event	The triggering event of the button press. Most likely a click event
		 * @return void
		 */
		'_triggerButtonPress' : function(e) {
			e.preventDefault();

			// only proceed if button is not disabled
			var $c = this.getControl(e.data.type);
			if ($c.data('disabled')) return;

			try {
				this[e.data.type]();
			} catch(err) {}
		},
		'play': function(id) {
			this._sendPlayerAction(id, 'PlayPause', {'play': true});
			this._updateGui();

			// fire event
			var e = $.Event("onActionPlay");
			this.fire.trigger(e, [this]);
		},

		'pause': function(id) {
			this._sendPlayerAction(id, 'PlayPause', {'play': false});
			this._updateGui();

			// fire event
			var e = $.Event("onActionPause");
			this.fire.trigger(e, [this]);
		},

		'playPause': function(id) {
			var p = this.getActivePlayer(id);
			if (p.isPlaying) {
				this.pause(id);
			} else {
				this.play(id);	
			}
		},

		'stop': function(id) {
			this._sendPlayerAction(id, 'Stop');
			this._updateGui();

			// fire event
			var e = $.Event("onActionStop");
			this.fire.trigger(e, [this]);
		},

		'previous': function(id) {
			this.goTo(id, 'previous');
			this._updateGui();

			// fire event
			var e = $.Event("onActionPrevious");
			this.fire.trigger(e, [this]);
		},

		'next': function(id) {
			this.goTo(id, 'next');
			this._updateGui();

			// fire event
			var e = $.Event("onActionNext");
			this.fire.trigger(e, [this]);
		},

		/**
		 * Tell player to go to a specific item in the playlist
		 *
		 * @param integer	id			The id of the player to control
		 * @param mixed	target
		 * @param object	params
		 * @return void
		 */
		'goTo': function(id, target, params) {
			this._sendPlayerAction(id, 'GoTo', $.extend( {}, params, {'to' : target} ));
		},

		/**
		 * Tell player to go to a specific item in the playlist
		 *
		 * @param integer	id			The id of the player to control
		 * @param mixed	item		Start playback of either the playlist with the given ID, a slideshow with the pictures from the given directory or a single file or an item from the database
		 * @param object	params
		 * @return void
		 */
		'open': function(id, item, params) {
			var p = this.getActivePlayer(id)
				override = {
					'item' : item
				};
			if (p) override.playerid = p.id;
			xbmc.rpc.request({
				'context': this,
				'method': 'Player.Open',
				'params': $.extend( {}, params, override)
			});
		},

		/**
		 * disables a control button
		 *
		 * @param string	type	The type of control to disable
		 * @return jQuery
		 */
		'disableControl': function(type) {
			var $c = this.getControl(type);
			if ($c.length) {
				$c.data('disabled', true)
					.addClass(this.options.classes.controls.disabled);
			}
			return $c;
		},

		/**
		 * enables a control button
		 *
		 * @param string	type	The type of control to disable
		 * @return jQuery
		 */
		'enableControl': function(type) {
			var $c = this.getControl(type);
			if ($c.length) {
				$c.data('disabled', false)
					.removeClass(this.options.classes.controls.disabled);
			}
			return $c;
		},

		/**
		 * returns the jQuery element for the given control type
		 *
		 * @param string	type	The type of control
		 * @return jQuery
		 */
		'getControl': function(type) {
			if (this.$controls && this.$controls.length) {
				return this.$controls.find(this.options.selector.controls[type]);
			}
			return $(this.options.selector.controls[type]);
		},

		/**
		 * returns the properties of the currently active player
		 *
		 * @param id	integer	The id of the desired player (optional)
		 * @return object The player object or false if no player is currently active
		 */
		'getActivePlayer': function(id) {
			if (!$(this.activePlayers).length) return false;

			if (typeof(id) !== 'undefined' && id > -1 && typeof(this.activePlayers[id]) == 'object') {
				return this.activePlayers[id];
			} else if ($(this.activePlayers).length) {
				for (var i in this.activePlayers) {
					return this.activePlayers[i];
					break; //fwiw
				}
			}
			return false;
		},

		/**
		 * returns the properties of the currently active player
		 *
		 * @param type	string	The type
		 * @return object The player object or false if no player is currently active
		 */
		'getActivePlayerByType': function(type) {
			if (!$(this.activePlayers).length || !type) return false;

			for (var i in this.activePlayers) {
				if (this.activePlayers[i].type == type) {
					return this.activePlayers[i];
				}
			}

			return false;
		},

		/**
		 * Sends a playback related action to xbmc
		 *
		 * @param integer		id	The id of the desired playlist (optional)
		 * @param string	type
		 * @param object	params
		 * @param function	callback		optional callback function
		 * @return void
		 */
		'_sendPlayerAction': function(id, action, params, callback) {
			var p = this.getActivePlayer(id);
			if (p && p.id > -1) {
				xbmc.rpc.request({
					'context': this,
					'method': 'Player.' + action,
					'params': $.extend( {}, params, {
						'playerid': p.id
					}),
					'success': callback
				});
			}
		},

		/**
		 * Updates the status of the given players. If no player ID is given, all active will be updated
		 *
		 * @param object	callbacks	Optional object with callback function on either success or error
		 * @return void
		 */
		'_updateActivePlayers': function(callbacks) {
			xbmc.rpc.request({
				'context': this,
				'method': 'Player.GetActivePlayers',
				'timeout': 3000,
				'success': function(data) {
					if (data && data.result && data.result.length) {
						var activePlayers = [];
						for (var i in data.result) {
							var p = data.result[i]
								, lastPlayerState = this.activePlayers[p.playerid];
							if (lastPlayerState) delete(lastPlayerState.lastState);
							activePlayers.push('id' + p.playerid); // we need the prefix because otherwise $.inArray would return -1 for player ID 0 even if it's in there
							this.activePlayers[p.playerid] = $.extend( {}, lastPlayerState, p, {id: p.playerid, lastState:lastPlayerState } );
						}
						// remove previously active players
						for (var i in this.activePlayers) {
							if ($.inArray('id' + i, activePlayers) == -1) {
								delete(this.activePlayers[i]);
							}
						}
					} else {
						this.activePlayers = {};
					}

					if (callbacks && typeof(callbacks.success) == 'function') callbacks.success(data, this);

					// fire event
					var e = $.Event("onActivePlayersUpdated");
					this.fire.trigger(e, ['success', data, this]);
				},
				'error': function(data, error) {
					xbmc.core.displayCommunicationError();
					if (callbacks && typeof(callbacks.error) == 'function') callbacks.error(data, this);

					// fire event
					var e = $.Event("onActivePlayersUpdated");
					this.fire.trigger(e, ['error', data, this]);
				}
			});
		},

		/**
		 * Updates the status of the player with the given id
		 *
		 * @param id	integer	The id of the desired player (optional)
		 * @param object	callbacks	Optional object with callback function on either success or error
		 * @return void
		 */
		'_updatePlayerStatus': function(id, callbacks) {
			var p = this.getActivePlayer(id);
			if (p && p.id > -1) {
				xbmc.rpc.request({
					'context': this,
					'method': 'Player.GetProperties',
					'timeout': 3000,
					'params': {
						'playerid': p.id,
						'properties': [
							'playlistid',
							'speed',
							'position',
							'totaltime',
							'time',
							'percentage',
							'canseek',
							'canrepeat',
							'canshuffle',
							'repeat',
							'shuffled'
						]
					},
					'success': function(data) {
						var state = 'error';
						if (data && data.result) {
							this.activePlayers[p.id] = $.extend(this.activePlayers[p.id], data.result, {
								isPlaying: data.result.speed != 0,
								isPaused: data.result.speed == 0
							});
							state = 'success';
						} else {
							delete(this.activePlayers[p.id]);
						}

						if (callbacks && typeof(callbacks[state]) == 'function') callbacks[state](data, this);

						// fire event
						var e = $.Event("onPlayerStatusUpdated");
						this.fire.trigger(e, [state, data, this.activePlayers[p.id], this]);
					},
					'error': function(data) {
						if (callbacks && typeof(callbacks.error) == 'function') callbacks.error(data, this);

						// fire event
						var e = $.Event("onPlayerStatusUpdated");
						this.fire.trigger(e, ['error', data, this.activePlayers[p.id], this]);
					}
				});
			}
		}
	};

	if (!xbmc.defaults) xbmc.defaults = {};
	xbmc.defaults.player = {
		selector : {
			controls : {
				'playPause'	: '[data-toggle="xbmc-player-playPause"]',
				'play'		: '[data-toggle="xbmc-player-play"]',
				'pause'		: '[data-toggle="xbmc-player-pause"]',
				'stop'		: '[data-toggle="xbmc-player-stop"]',
				'previous'	: '[data-toggle="xbmc-player-previous"]',
				'next'		: '[data-toggle="xbmc-player-next"]'
			}
		},
		classes : {
			controls : {
				'disabled'				: 'disabled',
				'playPausePlaying'	: 'playing',
				'playPausePaused'		: 'paused'
			}
		}
	}

	window.xbmc = xbmc;

}(window));

