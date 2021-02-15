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

#include <queue>
#include <atomic>
#include <string>

#include "NWTVAPI.h"
#include "NWClientUtilities.h"

#include "interfaces/IAnnouncer.h"
#include "threads/Thread.h"
#include "threads/CriticalSection.h"
#include "dialogs/GUIDialogProgress.h"
#include "utils/JobManager.h"
#include "utils/Stopwatch.h"

#define ENABLE_NWCLIENT_DEBUGLOGS 1

enum AssetDownloadState
{
  IsPresent,
  willDownload,
  wasDownloaded,
};

enum ClientStartupState
{
  ClientUseUpdateInterval,
  ClientFetchUpdatePlayer,
  ClientTryUseExistingPlayer,
};

// ---------------------------------------------
// ---------------------------------------------
typedef void (*ClientCallBackFn)(const void *ctx, int msg);
typedef void (*PlayerCallBackFn)(const void *ctx, int msg, struct NWAsset &asset);


class CNWPlayer;
class CNWMediaManager;
class CNWPurgeManager;

class CNWClient
: public CThread
, public CJobQueue
, public ANNOUNCEMENT::IAnnouncer
{
  friend class CNWClientJob;
public:
  CNWClient();
  virtual ~CNWClient();

  static CNWClient* GetClient();
  
  void Announce(ANNOUNCEMENT::AnnouncementFlag flag,
                const std::string& sender,
                const std::string& message,
                const CVariant& data) override;

  void          Startup(bool bypass_authorization, bool fetchAndUpdate);
  bool          IsPlaying();
  void          PlayPause();
  void          PausePlaying();
  void          StopPlaying();
  void          PlayNext();

  void          UpdateFromJson(std::string url, std::string machineID, std::string locationID);

  void          GetProgamInfo(NWPlaylist &playList) { playList = m_ProgramInfo; };
  void          GetPlayerInfo(NWPlayerInfo &playerInfo) { playerInfo = m_PlayerInfo; };
  
  void          RegisterClientCallBack(const void *ctx, ClientCallBackFn fn);
  void          RegisterPlayerCallBack(const void *ctx, PlayerCallBackFn fn);
  bool          DoAuthorize();
  bool          IsAuthorized();
  bool          AllowExit();
  bool          CheckUpdate();

protected:
  virtual void  Process();

  void          ShowStartUpDialog(bool fetchAndUpdate);
  void          CloseStartUpDialog();
  void          ManageStartupDialog();

  void          GetPlayerInfo();
  bool          GetProgamInfo();

  void          GetActions();
  void          ClearAction(TVAPI_Actions &actions, std::string id);
  void          SendFilesPlayed();
  void          LogFilesPlayed(std::string assetID,std::string assetFormat);
  void          SendFilesDownloaded();
  void          LogFilesDownLoaded(std::string assetID,std::string assetFormat);

  void          UpdateNetworkStatus();
  bool          SendPlayerStatus(const std::string status);

  void          InitializeInternalsFromPlayer();
  bool          CreatePlaylist(std::string home, NWPlaylist &playList,
                  const TVAPI_Playlist &playlist, const TVAPI_PlaylistItems &playlistItems);

  static void   AssetUpdateCallBack(const void *ctx, NWAsset &asset, AssetDownloadState downloadState);

  std::string   m_strHome;
  bool          m_HasNetwork;
  std::atomic<bool> m_Startup;
  ClientStartupState m_StartupState;
  bool          m_bypassDownloadWait;
  int           m_totalAssets;
  bool          m_assetsValidated;
  CDateTime     m_NextUpdateTime;
  CDateTimeSpan m_NextUpdateInterval;
  
  NWPlayerInfo  m_PlayerInfo;
  NWPlaylist    m_ProgramInfo;
  CNWPlayer     *m_Player;
  CNWMediaManager *m_MediaManager;
  CNWPurgeManager *m_PurgeManager;
  CGUIDialogProgress *m_dlgProgress;

  CCriticalSection m_reportLock;
  static CCriticalSection m_clientLock;
  static CNWClient *m_this;

  ClientCallBackFn m_ClientCallBackFn;
  const void      *m_ClientCallBackCtx;
  CStopWatch      m_updateTimer;
};
