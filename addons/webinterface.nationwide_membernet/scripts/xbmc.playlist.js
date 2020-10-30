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

	xbmc.playlist = function(element, player, options) {
		this.init(element, player, options);
	};

	xbmc.playlist.prototype = {
		constructor: xbmc.playlist,

		/**
		 * initializes the playlist object
		 *
		 * @param dom-node	The optional DOM node/jQuery object to bind this player to
		 * @param xbmc.player	Optional handle to the related player object. If not passed in the constructor you have to call the setPlayer method manually
		 * @param object		Optional overriding default options
		 * @return void
		 */
		'init': function(element, player, options) {
			this.$e = $(element);
			this.fire = this.$e.length ? this.$e.add(this) : $(this);
			this.options = $.extend(true, {}, xbmc.defaults.playlist, options, this.$e.data());
			this.playerService;
			this.currentPlaylist = -1;
			this.position = -1;
			this.timer;

			// callbacks
			var self = this;
			$.each(['onBeforeInit', 'onInit', 'onUpdate', 'onUpdateItem', 'onAdd', 'onClear'], function(i, name) {
				// configuration
				if ($.isFunction(self.options[name])) { 
					$(self).bind(name, self.options[name]); 
				}
			});

			// onBeforeInit
			var e = $.Event("onBeforeInit");
			this.fire.trigger(e, [this]);

			this.$container = this.$e.find(this.options.selector.itemContainer);
			this.$template = this.$e.find(this.options.selector.itemTemplate).detach();

			if (player) {
				this.setPlayer(player);
			}
			if (this.options.defaultPlaylist !== null) {
				if (this.getPlayer() && this.getPlayer().playlistid > -1) return;

				var type = typeof(this.options.defaultPlaylist);
				
				if (type == 'string') {
					xbmc.rpc.request({
						'method': 'Playlist.GetPlaylists',
						'success': function(data) {
							if (data && data.result) {
								for (var i in data.result) {
									if (data.result[i].type == self.options.defaultPlaylist) {
										self.setPlaylist(data.result[i].playlistid);
										break;
									}
								}
							}
						}
					});	
				} else if(type == 'number') {
					this.setPlaylist(this.options.defaultPlaylist)
				}
			}

			// onInit
			var e = $.Event("onInit");
			this.fire.trigger(e, [this]);
		},

		/**
		 * Sets the related player
		 *
		 * @param xbmc.player	player
		 * @return void
		 */
		'setPlayer': function(player) {
			var self = this;
			if (!player) return;

			this.playerService = player;

			// trigger necessary tasks after playerstatus was updated
			// @todo - make use websockets etc
			$(this.playerService)
				.bind('onPlayerStatusUpdated', function(e, status, result, playerData, obj) {
					if (status == 'success') {
						if (playerData.playlistid != self.currentPlaylist) {
							self.setPlaylist(playerData.playlistid);
						} else if (playerData.playlistid == self.currentPlaylist && playerData.playlistid > -1) {
							if (!self.timer) {
								self.timer = setTimeout( function() {
									self._updatePlaylistContent(self.currentPlaylist);
									self.timer = null;
								}, 5000);
							}
							if (playerData.position != self.position) {
								self.setCurrent(playerData.position);
							}
						} else {
							self.clear(true);
						}
					} else if (this.currentPlaylist > -1) {
						self.clear(true);
					}
				})
				.bind('onUpdate', function(e, playerData, $controls, obj) {
					var count = self.$container.children().length;
					if (count && self.getPlayer()) {
						if (self.position > 0) {
							obj.enableControl('previous');
						} else {
							obj.disableControl('previous');
						}
						if (self.position < count-1) {
							obj.enableControl('next');
						} else {
							obj.disableControl('next');
						}
					} else {
						self.setCurrent(-1);
						obj.disableControl('previous');
						obj.disableControl('next');
					}
					delete(count);
				});
		},

		/**
		 * Get the assigned player
		 *
		 * @return xbmc.player
		 */
		'getPlayer': function() {
			if (this.playerService) return this.playerService.getActivePlayer();
			return false;
		},

		/**
		 * Sets the id of the remote playlist this playlist object belongs to
		 *
		 * @param integer	id
		 * @return void
		 */
		'setPlaylist': function(id) {
			if (id == this.currentPlaylist) return;

			this.clear(true);
			this.currentPlaylist = id;

			if (this.currentPlaylist > -1) {
				this._updatePlaylistContent(this.currentPlaylist);
			}
		},

		/**
		 * Clears the playlist
		 *
		 * @param boolean	guiOnly	Flag if only gui should be cleared or the action should be forwarded to the client
		 * @return void
		 */
		'clear': function(guiOnly) {
			this.$container.empty();

			this.position = -1;
			if (!guiOnly) this._sendPlaylistAction(this.currentPlaylist, 'Clear');

			// fire event
			var e = $.Event("onClear");
			this.fire.trigger(e, [guiOnly, this]);
		},

		/**
		 * Update the playlists items
		 *
		 * @param object	data	The raw json query data
		 * @return void
		 */
		'update': function(data) {
			if (!data || !data.items) {
				return this.clear(true);
			}

			if (this.$container.children().length > data.items.length) {
				this.$container.children().slice(data.items.length).remove();
			}

			for (var i in data.items) {
				var $item = this.$container.children().eq(i);
				if (!$item.length) {
					this.add(data.items[i], true);
				} else {
					this.updateItem($item, data.items[i]);
				}
			}

			if (this.getPlayer()) {
				this.setCurrent(this.getPlayer().position);
			}

			// fire event
			var e = $.Event("onUpdate");
			this.fire.trigger(e, [data, this]);
		},

		/**
		 * Adds an item ot the playlist
		 *
		 * @param object	itemData	The item to add
		 * @param boolean	guiOnly	Flag that indicates if the action should be forwarded to the player
		 * @return void
		 */
		'add': function(itemData, guiOnly) {
			if (!itemData || !itemData.type) return;
			var self = this;

			$item = this.$template.clone()
						.appendTo(this.$container)
						.data('type', null);
			this.updateItem($item, itemData);

			$item.bind('click.xbmc', function(e) {
				e.preventDefault();
				var $i = $(this);
				if (self.playerService && $i.data) {
					if (self.getPlayer()) {
						self.playerService.goTo(self.getPlayer(), $i.index());
					} else {
						self.playerService.open(null, {'playlistid': self.currentPlaylist, 'position': $i.index()});
					}
				}
			});

			// fire event
			var e = $.Event("onAdd");
			this.fire.trigger(e, [itemData, guiOnly, this]);

			if(!guiOnly) this._sendPlaylistAction(this.currentPlaylist, 'Add', {'item':itemData});
		},

		/**
		 * Sets the currently playing item
		 *
		 * @param integer	position
		 * @return void
		 */
		'setCurrent': function(position) {
			this.position = position;
			this.$container.children()
				.removeClass(this.options.classes.current);
			if (position > -1) {
				this.$container.children()
					.eq(position)
					.addClass(this.options.classes.current);
			}
		},

		/**
		 * Update the data of the given playlist item
		 *
		 * @param DOM/jQuery	item	The item to update
		 * @param object		data	The new item data
		 * @return void
		 */
		'updateItem': function(item, data) {
			var $i = $(item)
				, id;
			if (!$i.length) return;
			if (!data) return $i.remove();

			id = data.type + data.id;
			if ($i.data('id') && $i.data('id') == id) return;

			$i.data('id', id)
				.data('data', data);

			// fill placeholders
			var $properties = $i.find('[data-property]');
			$properties.each(function() {
				var $p = $(this)
					, properties = $p.data('property').split(',')
					, i;
				for (i in properties) {
					if (data[ properties[i] ]) {
						$p.text(data[ properties[i] ]);
						break;
					}
				}
			});

			// fire event
			var e = $.Event("onUpdateItem");
			this.fire.trigger(e, [$i, data, this]);
		},

		/**
		 * Sends a playlist related action to xbmc
		 *
		 * @param integer		id	The id of the desired playlist (optional)
		 * @param string		type
		 * @param object		params
		 * @param function	callback		optional callback function
		 * @return void
		 */
		'_sendPlaylistAction': function(id, action, params, callback) {
			var pid = id || this.currentPlaylist;
			if (pid > -1) {
				xbmc.rpc.request({
					'method': 'Playlist.' + action,
					'params': $.extend( {}, params, {
						'playlistid': pid
					}),
					'success': callback
				});
			}
		},

		/**
		 * Gets the data of the given playlist from the client
		 *
		 * @param integer	id				The id of the playlist the data should be fetched for
		 * @param object	callbacks	Optional object with callback function on either success or error
		 * @return void
		 */
		'_updatePlaylistContent': function(id, callbacks) {
			var pid = typeof(id) == 'number' ? id : this.currentPlaylist;
			if (pid > -1) {
				xbmc.rpc.request({
					'context': this,
					'method': 'Playlist.GetItems',
					'params': {
						'playlistid': pid,
						'properties': [
							'uniqueid',
							'title',
							'artist',
							'albumartist',
							'genre',
							'year',
							'album',
							'track',
							'duration',
							'studio',
							'displayartist',
							'albumlabel'
						]
					},
					'success': function(data) {
						var state = 'error';
						if (data && data.result && data.result.items) {
							state = 'success';
							this.update(data.result);
						} else {
							this.clear(true);
						}

						if (callbacks && typeof(callbacks[state]) == 'function') callbacks[state](data, this);

						// fire event
						var e = $.Event("onPlaylistContentUpdated");
						this.fire.trigger(e, [state, data, this]);
					},
					'error': function(data) {
						if (callbacks && typeof(callbacks.error) == 'function') callbacks.error(data, this);

						// fire event
						var e = $.Event("onPlaylistContentUpdated");
						this.fire.trigger(e, ['error', data, this]);
					}
				});
			}
		}
	};

	if (!xbmc.defaults) xbmc.defaults = {};
	xbmc.defaults.playlist = {
		defaultPlaylist		: null,
		selector : {
			'itemContainer'	: '[data-type="xbmc-playlist-itemcontainer"]',
			'itemTemplate'		: '[data-type="xbmc-playlist-itemtemplate"]'
		},
		classes : {
			'disabled'			: 'disabled',
			'current'			: 'current'
		}
	}

	window.xbmc = xbmc;

}(window));