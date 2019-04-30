#pragma once

/*
 *  Copyright (C) 2016 RootCoder, LLC.
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
 *  along with this app; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <atomic>

#include "XBDateTime.h"
#include "threads/Thread.h"
#include "threads/CriticalSection.h"

#include "NWClient.h"

#define ENABLE_NWPLAYER_DEBUGLOGS 1

typedef void (*PlayerCallBackFn)(const void *ctx, int msg, struct NWAsset &asset);

class CNWPlayer
: public CThread
{
public:
  CNWPlayer();
  virtual ~CNWPlayer();

  void          Reset();
  void          Play();
  void          PlayPause();
  void          PlayNext();
  void          Pause();
  bool          IsPlaying();
  void          StopPlaying();
  void          OverridePlayBackWindow(bool override);
  void          SetPlayBackTime(const CDateTime &time, const CDateTimeSpan &duration);
  void          MarkValidated(struct NWAsset &asset);
  void          QueueProgramInfo(NWPlaylist &playlist);
  void          RegisterPlayerCallBack(const void *ctx, PlayerCallBackFn fn);

protected:
  virtual void  Process();
  bool          Exists(NWGroup &group);

  std::atomic<bool>     m_playing;
  CCriticalSection      m_media_lock;
  CCriticalSection      m_player_lock;
  NWPlaylist            m_playlist;
  bool                  m_Override;
  CDateTime             m_PlaybackTime;
  CDateTimeSpan         m_PlayBackDuration;

  PlayerCallBackFn      m_PlayerCallBackFn;
  const void           *m_PlayerCallBackCtx;
};
