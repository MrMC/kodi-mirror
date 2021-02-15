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

#include "system.h"

#include "NWTVAPI.h"
#include "NWClient.h"
#include "NWPlayer.h"
#include "NWMediaManager.h"
#include "NWPurgeManager.h"

#include "Application.h"
#include "messaging/ApplicationMessenger.h"
#include "Util.h"
#include "ServiceBroker.h"
#include "guilib/GUIComponent.h"
#include "URL.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#include "filesystem/SpecialProtocol.h"
#include "guilib/GUIWindowManager.h"
#include "interfaces/AnnouncementManager.h"
#include "network/Network.h"
#include "settings/Settings.h"
#include "utils/FileUtils.h"
#include "utils/log.h"
#include "utils/TimeUtils.h"
#include "utils/StringUtils.h"
#include "utils/SystemInfo.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML.h"
#include "utils/XMLUtils.h"
#include "utils/JSONVariantParser.h"

#include "guilib/GUIKeyboardFactory.h"
#include "guilib/GUIWindowManager.h"
#include "settings/MediaSourceSettings.h"
#include "storage/MediaManager.h"

#include "settings/SettingsComponent.h"

// temp until access moves to core
#if defined(TARGET_ANDROID)
#include "platform/android/jni/Context.h"
#include "platform/android/jni/SettingsSecure.h"
#else
#include "platform/darwin/DarwinUtils.h"
#endif

#include <string>
#include <sstream>

void Parse(int result[4], const std::string& input)
{
    std::istringstream parser(input);
    parser >> result[0];
    for(int idx = 1; idx < 4; idx++)
    {
        parser.get(); //Skip period
        parser >> result[idx];
    }
}

bool LessThanVersion(const std::string& a,const std::string& b)
{
    int parsedA[4], parsedB[4];
    Parse(parsedA, a);
    Parse(parsedB, b);
    return std::lexicographical_compare(parsedA, parsedA + 4, parsedB, parsedB + 4);
}

template <typename T>
static std::string std_to_string(T value)
{
#if defined(TARGET_ANDROID)
  std::ostringstream os;
  os << value;
  return os.str();
#else
  return std::to_string(value);
#endif
}

static int std_stoi(std::string value)
{
  return atoi(value.c_str());
}

class CNWClientJob: public CJob
{
public:
  CNWClientJob(CNWClient *nwmnClient, std::string strFunction)
  : m_function(strFunction)
  , m_client(nwmnClient)
  {
  }
  virtual ~CNWClientJob()
  {
  }
  virtual bool DoWork()
  {
    if (m_function == "GetActions")
    {
      #if ENABLE_NWCLIENT_DEBUGLOGS
      CLog::Log(LOGINFO, "**NW** - CNWClientJob: GetActions");
      #endif
      m_client->GetActions();
      return true;
    }
    return false;
  }
  virtual bool operator==(const CJob *job) const
  {
    return true;
  }
private:

  std::string    m_function;
  CNWClient     *m_client;
};

CCriticalSection CNWClient::m_clientLock;
CNWClient *CNWClient::m_this = NULL;

CNWClient::CNWClient()
 : CThread("CNWClient")
 , m_HasNetwork(true)
 , m_Startup(true)
 , m_StartupState(ClientFetchUpdatePlayer)
 , m_bypassDownloadWait(false)
 , m_NextUpdateTime(CDateTime::GetCurrentDateTime())
 , m_NextUpdateInterval(0, 0, 5, 0)
 , m_Player(NULL)
 , m_MediaManager(NULL)
 , m_PurgeManager(NULL)
 , m_ClientCallBackFn(NULL)
 , m_ClientCallBackCtx(NULL)
 , m_updaterRunning(false)
{
  CLog::Log(LOGDEBUG, "**NW** - NW version %f", kNWClient_PlayerFloatVersion);

  // default path to local red directory in home
  std::string home = "special://home/nwmn/";

  // look for a removable disk with an
  // existing nwmn directory
  VECSOURCES sources;
  CServiceBroker::GetMediaManager().GetRemovableDrives(sources);
  for (size_t indx = 0; indx < sources.size(); indx++)
  {
    std::string test_dir = sources[indx].strPath + "/nwmn/";
    if (XFILE::CFile::Exists(test_dir))
    {
      // found one, use it.
      home = test_dir;
      break;
    }
  }
  CSpecialProtocol::SetCustomPath("nwmn", home);
  CLog::Log(LOGDEBUG, "**NW** - NW special:// path - %s", home.c_str());

    // now we can ref to either location using the same special path.
  m_strHome = "special://nwmn/";
  if (!XFILE::CFile::Exists(m_strHome))
    CUtil::CreateDirectoryEx(m_strHome);

  std::string download_path = m_strHome + kNWClient_DownloadPath;
  if (!XFILE::CFile::Exists(download_path))
    CUtil::CreateDirectoryEx(download_path);

  std::string download_log_path = m_strHome + kNWClient_LogPath;
  if (!XFILE::CFile::Exists(download_log_path))
    CUtil::CreateDirectoryEx(download_log_path);

  std::string video_path = m_strHome + kNWClient_DownloadVideoPath;
  if (!XFILE::CFile::Exists(video_path))
    CUtil::CreateDirectoryEx(video_path);

  std::string videothumbs_path = m_strHome + kNWClient_DownloadVideoThumbNailsPath;
  if (!XFILE::CFile::Exists(videothumbs_path))
    CUtil::CreateDirectoryEx(videothumbs_path);
/*
  std::string music_path = m_strHome + kNWClient_DownloadMusicPath;
  if (!XFILE::CFile::Exists(music_path))
    CUtil::CreateDirectoryEx(music_path);

  std::string musicthumbs_path = m_strHome + kNWClient_DownloadMusicThumbNailsPath;
  if (!XFILE::CFile::Exists(musicthumbs_path))
    CUtil::CreateDirectoryEx(musicthumbs_path);
*/
  std::string webui_path = m_strHome + "webdata/";
  if (!XFILE::CFile::Exists(webui_path))
    CUtil::CreateDirectoryEx(webui_path);

  std::string strCertPath = m_strHome + kNWClient_CertPath;
  if (!XFILE::CFile::Exists(strCertPath))
    CUtil::CreateDirectoryEx(strCertPath);

  m_HasNetwork = HasInternet();

  TVAPI_SetURLBASE(kTVAPI_URLBASE);
  LoadLocalPlayer(m_strHome, m_PlayerInfo);
  InitializeInternalsFromPlayer();

  m_MediaManager = new CNWMediaManager();
  m_MediaManager->RegisterAssetUpdateCallBack(this, AssetUpdateCallBack);
  m_PurgeManager = new CNWPurgeManager();
  m_PurgeManager->AddPurgePath(video_path, videothumbs_path);
  m_Player = new CNWPlayer();

  m_dlgProgress = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogProgress>(WINDOW_DIALOG_PROGRESS);

  CSingleLock lock(m_clientLock);
  m_this = this;

  CServiceBroker::GetAnnouncementManager()->AddAnnouncer(this);
}

CNWClient::~CNWClient()
{
  CSingleLock lock(m_clientLock);
  m_this = NULL;
  m_dlgProgress = NULL;
  m_ClientCallBackFn = NULL;
  m_bStop = true;
  StopThread();

  CServiceBroker::GetAnnouncementManager()->RemoveAnnouncer(this);
  SendPlayerStatus(kTVAPI_Status_Off);

  SAFE_DELETE(m_Player);
  SAFE_DELETE(m_MediaManager);
  SAFE_DELETE(m_PurgeManager);
}

CNWClient* CNWClient::GetClient()
{
  CSingleLock lock(m_clientLock);
  return m_this;
}

void CNWClient::Announce(ANNOUNCEMENT::AnnouncementFlag flag, const std::string &sender, const std::string &message, const CVariant &data)
{
  if (sender != "xbmc")
    return;

  if (flag == ANNOUNCEMENT::Player)
  {
    if (message == "OnPlay")
    {
      #if ENABLE_NWCLIENT_DEBUGLOGS
      CLog::Log(LOGDEBUG, "**MN** - CNWClient::Announce() - Playback started");
      #endif
      CFileItem currentFile(g_application.CurrentFileItem());
      std::string strPath = currentFile.GetPath();
      std::string assetID = URIUtils::GetFileName(strPath);
      URIUtils::RemoveExtension(assetID);
      std::string format = currentFile.GetProperty("video_format").asString();
      LogFilesPlayed(assetID,format);
    }
    else if (message == "OnStop")
    {
      if (data.isMember("end") && data["end"] == false)
      {
        #if ENABLE_NWCLIENT_DEBUGLOGS
        CLog::Log(LOGDEBUG, "**MN** - CNWClient::Announce() - Playback stopped");
        #endif
        // playback stopped, someone hit back in gui.
        StopThread();
        StopPlaying();
        m_totalAssets = 0;
      }
    }
  }
}

void CNWClient::Startup(bool bypass_authorization, bool fetchAndUpdate)
{
  CLog::Log(LOGDEBUG, "**NW** - CNWClient::Startup, bypass_authorization(%d), fetchAndUpdate(%d)"
    ,bypass_authorization, fetchAndUpdate);

  ShowStartUpDialog(fetchAndUpdate);

  StopThread();
  StopPlaying();
  m_totalAssets = 0;

  if (!bypass_authorization)
  {
    if (HasInternet() && !IsAuthorized())
      while (!DoAuthorize());
  }

  SendPlayerStatus(kTVAPI_Status_Restarting);

  m_Startup = true;
  m_bypassDownloadWait = false;
  m_StartupState = ClientFetchUpdatePlayer;
  if (!fetchAndUpdate)
  {
    if (HasLocalPlayer(m_strHome) && HasLocalPlaylist(m_strHome))
    {
      m_bypassDownloadWait = true;
      m_StartupState = ClientTryUseExistingPlayer;
    }
  }

  Create();
  if (!m_MediaManager->IsRunning())
    m_MediaManager->Create();
  if (!m_PurgeManager->IsRunning())
    m_PurgeManager->Create();
  if (!m_Player->IsRunning())
    m_Player->Create();
}

bool CNWClient::IsPlaying()
{
  return m_Player->IsPlaying();
}

void CNWClient::PlayPause()
{
  m_Player->PlayPause();
}

void CNWClient::PausePlaying()
{
  m_Player->Pause();
}

void CNWClient::StopPlaying()
{
  if (m_Player->IsPlaying())
    m_Player->StopPlaying();
  if (m_MediaManager)
  {
    m_MediaManager->ClearDownloads();
    m_MediaManager->ClearAssets();
  }
}

void CNWClient::PlayNext()
{
  m_Player->PlayNext();
}

void CNWClient::RegisterClientCallBack(const void *ctx, ClientCallBackFn fn)
{
  m_ClientCallBackFn = fn;
  m_ClientCallBackCtx = ctx;
}

void CNWClient::UpdateFromJson(std::string url, std::string machineID, std::string locationID)
{
}

void CNWClient::RegisterPlayerCallBack(const void *ctx, PlayerCallBackFn fn)
{
  m_Player->RegisterPlayerCallBack(ctx, fn);
}

void CNWClient::Process()
{
  SetPriority(THREAD_PRIORITY_BELOW_NORMAL);
  #if ENABLE_NWCLIENT_DEBUGLOGS
  CLog::Log(LOGDEBUG, "**NW** - CNWClient::Process Started");
  #endif

  /// check for update every 6 hours?? and check on every startup
  if (!m_updaterRunning && (!m_updateTimer.IsRunning() ||
      (m_updateTimer.IsRunning() && m_updateTimer.GetElapsedMilliseconds() > 21600.0f)))
  {
    m_updateTimer.StartZero();
    CNWClient::CheckUpdate();
  }

  SendPlayerStatus(kTVAPI_Status_On);

  while (!m_bStop)
  {
    if (!m_Startup)
      Sleep(100);
    else
      Sleep(1);

    if (m_Startup)
      ManageStartupDialog();

    CDateTime time = CDateTime::GetCurrentDateTime();
    if (m_StartupState != ClientUseUpdateInterval || time >= m_NextUpdateTime)
    {
      #if ENABLE_NWCLIENT_DEBUGLOGS
      CLog::Log(LOGDEBUG, "**NW** - (process) time = %s", time.GetAsDBDateTime().c_str());
      CLog::Log(LOGDEBUG, "**NW** - (process) m_NextUpdateTime = %s", m_NextUpdateTime.GetAsDBDateTime().c_str());
      CLog::Log(LOGDEBUG, "**NW** - (process) m_NextUpdateInterval = %d days, %d hours, %d mins",
        m_NextUpdateInterval.GetDays(), m_NextUpdateInterval.GetHours(), m_NextUpdateInterval.GetMinutes());
      #endif
      m_NextUpdateTime += m_NextUpdateInterval;
      CLog::Log(LOGDEBUG, "**NW** - (process) new m_NextUpdateTime = %s", m_NextUpdateTime.GetAsDBDateTime().c_str());

      UpdateNetworkStatus();
      if (m_HasNetwork)
        AddJob(new CNWClientJob(this, "GetActions"));

      GetPlayerInfo();
      if (GetProgamInfo())
      {
        m_StartupState = ClientUseUpdateInterval;
        if (m_bypassDownloadWait)
          m_Player->Play();
      }

      if (m_ClientCallBackFn)
        (*m_ClientCallBackFn)(m_ClientCallBackCtx, 0);

      if (m_Player->IsPlaying())
        SendPlayerStatus(kTVAPI_Status_Playing);
      else
        SendPlayerStatus(kTVAPI_Status_On);
    }
  }

  #if ENABLE_NWCLIENT_DEBUGLOGS
  CLog::Log(LOGDEBUG, "**NW** - CNWClient::Process Stopped");
  #endif
}

void CNWClient::ShowStartUpDialog(bool fetchAndUpdate)
{
  m_dlgProgress->SetHeading("Player Startup");
  if (!fetchAndUpdate && HasLocalPlayer(m_strHome) && HasLocalPlaylist(m_strHome))
    m_dlgProgress->SetLine(1, StringUtils::Format("Verifying player and media files"));
  else
    m_dlgProgress->SetLine(1, StringUtils::Format("Download/Verify player and media files"));
  m_dlgProgress->Open();
  m_dlgProgress->ShowProgressBar(true);
}

void CNWClient::CloseStartUpDialog()
{
  m_dlgProgress->Close();
  while (m_dlgProgress->IsDialogRunning())
    Sleep(10);
}

void CNWClient::ManageStartupDialog()
{
  if (!m_bypassDownloadWait)
  {
    CLog::Log(LOGDEBUG, "**NW** - CNWClient::ManageStartupDialog m_bypassDownloadWait is false");
    if (m_dlgProgress->IsCanceled())
    {
      m_Startup = false;
      m_StartupState = ClientUseUpdateInterval;
      CloseStartUpDialog();

      StopPlaying();

      if (m_ClientCallBackFn)
        (*m_ClientCallBackFn)(m_ClientCallBackCtx, 1);
      SendPlayerStatus(kTVAPI_Status_On);
    }
    else if (m_dlgProgress->IsDialogRunning())
    {
      if (m_MediaManager->GetLocalAssetCount() > 0 && m_MediaManager->GetDownloadCount() == 0)
      {
        m_Startup = false;
        CloseStartUpDialog();
        m_Player->Play();
        if (m_ClientCallBackFn)
          (*m_ClientCallBackFn)(m_ClientCallBackCtx, 2);
      }
    }
  }
  else
  {
    CLog::Log(LOGDEBUG, "**NW** - CNWClient::ManageStartupDialog m_bypassDownloadWait is true");
    // when starting up, two choices
    // 1) dialog was canceled -> return to main window and idle
    // 2) dialog is up -> waiting for download, verify and start of playback
    if (m_dlgProgress->IsCanceled())
    {
      m_Startup = false;
      m_StartupState = ClientUseUpdateInterval;
      CloseStartUpDialog();

      StopPlaying();

      if (m_ClientCallBackFn)
        (*m_ClientCallBackFn)(m_ClientCallBackCtx, 1);
      SendPlayerStatus(kTVAPI_Status_On);
    }
    else if (g_application.GetAppPlayer().IsPlaying() && m_dlgProgress->IsDialogRunning())
    {
      m_Startup = false;
      CloseStartUpDialog();
      if (m_ClientCallBackFn)
        (*m_ClientCallBackFn)(m_ClientCallBackCtx, 2);
    }
  }
}

void CNWClient::GetPlayerInfo()
{
  if (m_HasNetwork && m_StartupState != ClientTryUseExistingPlayer)
  {
    CLog::Log(LOGDEBUG, "**NW** - CNWClient::GetPlayerInfo fetching player info");
    TVAPI_Machine machine;
    machine.apiKey = m_PlayerInfo.apiKey;
    machine.apiSecret = m_PlayerInfo.apiSecret;
    TVAPI_GetMachine(machine);

    m_PlayerInfo.id  = machine.id;
    m_PlayerInfo.name = machine.machine_name;
    m_PlayerInfo.vendor = machine.vendor;
    m_PlayerInfo.member = machine.member;
    m_PlayerInfo.timezone = machine.timezone;
    m_PlayerInfo.description = machine.description;
    m_PlayerInfo.serial_number = machine.serial_number;
    m_PlayerInfo.warranty_number = machine.warranty_number;
    m_PlayerInfo.macaddress = GetWiredMACAddress();
    m_PlayerInfo.macaddress_wireless = GetWirelessMACAddress();
    m_PlayerInfo.hardware_version = "";
    m_PlayerInfo.software_version = kNWClient_PlayerFloatVersion * 10;

    m_PlayerInfo.playlist_id = machine.playlist_id;
    m_PlayerInfo.video_format = machine.video_format;
    m_PlayerInfo.update_time = machine.update_time;
    m_PlayerInfo.update_interval = machine.update_interval;

    m_PlayerInfo.allow_new_content = machine.allow_new_content;
    m_PlayerInfo.allow_software_update = machine.allow_software_update;
    m_PlayerInfo.allow_async_player = "no";
    m_PlayerInfo.status = machine.status;
    m_PlayerInfo.apiKey = machine.apiKey;
    m_PlayerInfo.apiSecret = machine.apiSecret;
    m_PlayerInfo.tvapiURLBase = TVAPI_GetURLBASE();
    InitializeInternalsFromPlayer();

    SaveLocalPlayer(m_strHome, m_PlayerInfo);
  }
  else
  {
    if (HasLocalPlayer(m_strHome))
    {
      LoadLocalPlayer(m_strHome, m_PlayerInfo);
      InitializeInternalsFromPlayer();
    }
  }
}

bool CNWClient::GetProgamInfo()
{
  bool rtn = false;

  if (m_HasNetwork && m_StartupState != ClientTryUseExistingPlayer)
  {
    TVAPI_Playlist playlist;
    playlist.apiKey = m_PlayerInfo.apiKey;
    playlist.apiSecret = m_PlayerInfo.apiSecret;
    TVAPI_GetPlaylist(playlist, m_PlayerInfo.playlist_id);

    bool updateProgram = false;
    // if the fetched player's playlist id does not match the current
    if (std_stoi(m_PlayerInfo.playlist_id) != m_ProgramInfo.id)
    {
      CLog::Log(LOGDEBUG, "**NW** - CNWClient::GetProgamInfo playlist_id changed");
      updateProgram = true;
    }
    // if the fetched playlist modified date does not match the current.
    if (m_ProgramInfo.updated_date != playlist.updated_date)
    {
      CLog::Log(LOGDEBUG, "**NW** - CNWClient::GetProgamInfo updated_date changed");
      updateProgram = true;
    }
    // if we are forcing the update (ClientFetchUpdatePlayer or ClientTryUseExistingPlayer)
    if (m_StartupState != ClientUseUpdateInterval)
    {
      CLog::Log(LOGDEBUG, "**NW** - CNWClient::GetProgamInfo not ClientUseUpdateInterval");
      updateProgram = true;
    }

    if (updateProgram)
    {
      CLog::Log(LOGDEBUG, "**NW** - CNWClient::GetProgamInfo fetching update");
      TVAPI_PlaylistItems playlistItems;
      playlistItems.apiKey = m_PlayerInfo.apiKey;
      playlistItems.apiSecret = m_PlayerInfo.apiSecret;
      TVAPI_GetPlaylistItems(playlistItems, m_PlayerInfo.playlist_id);

      m_ProgramInfo.video_format = m_PlayerInfo.video_format;
      CreatePlaylist(m_strHome, m_ProgramInfo, playlist, playlistItems);
      SaveLocalPlaylist(m_strHome, m_ProgramInfo);

      // send msg to GUIWindowMN to change to horz/vert
      // see m_ProgramInfo.layout
      // make sure control id's match those in CGUIWindowMN.cpp
      if (m_ProgramInfo.layout == "horizontal")
      {
        // if we are vertical, switch to horizontal
        if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::MN_VERTICAL))
        {
          CGUIMessage msg(GUI_MSG_CLICKED, 90144, WINDOW_MEMBERNET);
          CServiceBroker::GetGUI()->GetWindowManager().SendMessage(msg);
        }
      }
      else if (m_ProgramInfo.layout == "vertical")
      {
        // if we are horizontal, switch to vertical
        if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::MN_VERTICAL))
        {
          CGUIMessage msg(GUI_MSG_CLICKED, 90134, WINDOW_MEMBERNET);
          CServiceBroker::GetGUI()->GetWindowManager().SendMessage(msg);
        }
      }

      // queue all assets belonging to this playlist
      m_Player->QueueProgramInfo(m_ProgramInfo);
      int total_assets = 0;
      m_MediaManager->ClearAssets();
      for (auto group : m_ProgramInfo.groups)
      {
        if (!group.assets.empty())
        {
          total_assets += group.assets.size();
          m_MediaManager->QueueAssetsForDownload(group.assets);
        }
      }
      m_totalAssets = total_assets;

      rtn = true;
    }
  }
  else
  {
    if (HasLocalPlaylist(m_strHome))
    {
      std::string updated_date = m_ProgramInfo.updated_date;
      if (LoadLocalPlaylist(m_strHome, m_ProgramInfo))
      {
        if (m_StartupState != ClientUseUpdateInterval || m_ProgramInfo.updated_date != updated_date)
        {
          // send msg to GUIWindowMN to change to horz/vert
          // see m_ProgramInfo.layout
          // make sure control id's match those in CGUIWindowMN.cpp
          if (m_ProgramInfo.layout == "horizontal")
          {
            // if we are vertical, switch to horizontal
            if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::MN_VERTICAL))
            {
              CGUIMessage msg(GUI_MSG_CLICKED, 90144, WINDOW_MEMBERNET);
              CServiceBroker::GetGUI()->GetWindowManager().SendMessage(msg);
            }
          }
          else if (m_ProgramInfo.layout == "vertical")
          {
            // if we are horizontal, switch to vertical
            if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::MN_VERTICAL))
            {
              CGUIMessage msg(GUI_MSG_CLICKED, 90134, WINDOW_MEMBERNET);
              CServiceBroker::GetGUI()->GetWindowManager().SendMessage(msg);
            }
          }

          // queue all assets belonging to this mediagroup
          m_Player->QueueProgramInfo(m_ProgramInfo);
          int total_assets = 0;
          m_MediaManager->ClearAssets();
          for (auto group : m_ProgramInfo.groups)
          {
            if (!group.assets.empty())
            {
              total_assets += group.assets.size();
              m_MediaManager->QueueAssetsForDownload(group.assets);
            }
          }
          m_totalAssets = total_assets;
        }
      }
      // if we are off-line, always return true so m_FullUpdate will get set right
      rtn = true;
    }
  }

  return rtn;
}

void CNWClient::GetActions()
{
  if (!m_HasNetwork)
    return;

  TVAPI_Actions actions;
  bool filePlayedSent = false;
  actions.apiKey = m_PlayerInfo.apiKey;
  actions.apiSecret = m_PlayerInfo.apiSecret;
  if (TVAPI_GetActionQueue(actions))
  {
    for (auto action: actions.actions)
    {
      if (action.action == kTVAPI_ActionHealth)
      {
        TVAPI_HealthReport health;
        health.apiKey = m_PlayerInfo.apiKey;
        health.apiSecret = m_PlayerInfo.apiSecret;
        health.date = CDateTime::GetCurrentDateTime().GetAsDBDateTime();
        health.uptime = GetSystemUpTime();
        health.disk_used = GetDiskUsed("/");
        health.disk_free = GetDiskFree("/");
        health.smart_status = "Disk Ok";
        TVAPI_ReportHealth(health);
        ClearAction(actions, action.id);
      }
      else if (action.action == kTVAPI_ActionDownloaded)
      {
        SendFilesDownloaded();
        ClearAction(actions, action.id);
      }
      else if (action.action == kTVAPI_ActionFilePlayed)
      {
        SendFilesPlayed();
        ClearAction(actions, action.id);
        filePlayedSent = true;
      }
      else if (action.action == kTVAPI_ActionPlay)
      {
        // do nothing, this action is badly defined
        CLog::Log(LOGINFO, "**NW** - GotAction kTVAPI_ActionPlay");
        ClearAction(actions, action.id);
      }
      else if (action.action == kTVAPI_ActionStop)
      {
        // do nothing, this action is badly defined
        CLog::Log(LOGINFO, "**NW** - GotAction kTVAPI_ActionStop");
        ClearAction(actions, action.id);
      }
      else if (action.action == kTVAPI_ActionRestart)
      {
        // flip the order so server action gets cleared
        //ClearAction(actions, action.id);
        //CApplicationMessenger::Get().Restart();
      }
      else if (action.action == kTVAPI_ActionSwitchOff)
      {
      }
    }
    actions.actions.clear();
  }

  // 'files played' action must be uploaded regardless of if there was
  // an action for it or not.
  if (!filePlayedSent)
  {
    SendFilesPlayed();
  }
}

void CNWClient::ClearAction(TVAPI_Actions &actions, std::string id)
{
  if (!m_HasNetwork)
    return;

  // clear a server side requested action
  for (auto action: actions.actions)
  {
    if (action.id == id)
    {
      TVAPI_ActionStatus actionStatus;
      actionStatus.apiKey = m_PlayerInfo.apiKey;
      actionStatus.apiSecret = m_PlayerInfo.apiSecret;
      actionStatus.id = action.id;
      actionStatus.status = kTVAPI_ActionStatusCompleted;
      TVAPI_UpdateActionStatus(actionStatus);
      break;
    }
  }
}

void CNWClient::SendFilesPlayed()
{
  if (!m_HasNetwork)
    return;


  std::string filename = m_strHome + "log/" + m_PlayerInfo.id + "_playback.log";
  if (XFILE::CFile::Exists(filename))
  {
    CSingleLock lock(m_reportLock);

    XFILE::CFile file;
    XFILE::auto_buffer buffer;
    file.LoadFile(filename, buffer);
    file.Close();

    TVAPI_Files playedFiles;
    playedFiles.apiKey = m_PlayerInfo.apiKey;
    playedFiles.apiSecret = m_PlayerInfo.apiSecret;

    std::string charbuffer(buffer.get());
    // the file buffer is NOT null terminated :)
    charbuffer[buffer.size()] = 0x00;
    std::vector<std::string> lines = StringUtils::Split(charbuffer, '\n');
    for (auto line: lines)
    {
      if (line.empty())
        continue;
      std::vector<std::string> items = StringUtils::Split(line, ',');
      TVAPI_File playedFile;
      switch(items.size())
      {
          case 3:
            playedFile.format = items[2];
            // no break so drop through to next
          case 2:
            playedFile.date = items[0];
            playedFile.id = items[1];
            playedFiles.files.push_back(playedFile);
            break;
          default:
            if (!line.empty() && line[0] != 0x00)
              CLog::Log(LOGDEBUG, "**NW** - SendFilesPlayed, split failed = %s", line.c_str());
            break;
      }
    }

    if (!playedFiles.files.empty())
    {
      if (TVAPI_ReportFilesPlayed(playedFiles, m_PlayerInfo.serial_number))
        XFILE::CFile::Delete(filename);
    }
  }
}

void CNWClient::LogFilesPlayed(std::string assetID,std::string assetFormat)
{
  //  date,assetID,type
  //  2015-02-05 12:01:40-0500,58350,4K
  //  2015-02-05 12:05:40-0500,57116,720

  CSingleLock lock(m_reportLock);
  CDateTime time = CDateTime::GetCurrentDateTime();
  std::string strData = StringUtils::Format("%s,%s,%s\n",
    time.GetAsDBDateTime().c_str(),
    assetID.c_str(),
    assetFormat.c_str()
  );

  std::string filename = m_strHome + "log/" + m_PlayerInfo.id + "_playback.log";
  XFILE::CFile file;
  file.OpenForWrite(filename);
  file.Seek(0, SEEK_END);
  file.Write(strData.c_str(), strData.size());
  file.Close();
}

void CNWClient::SendFilesDownloaded()
{
  if (!m_HasNetwork)
    return;

  std::string filename = m_strHome + "log/" + m_PlayerInfo.id + "_download.log";
  if (XFILE::CFile::Exists(filename))
  {
    CSingleLock lock(m_reportLock);

    XFILE::CFile file;
    XFILE::auto_buffer buffer;
    file.LoadFile(filename, buffer);
    file.Close();

    TVAPI_Files downloadedFiles;
    downloadedFiles.apiKey = m_PlayerInfo.apiKey;
    downloadedFiles.apiSecret = m_PlayerInfo.apiSecret;

    std::string charbuffer(buffer.get());
    // the file buffer is NOT null terminated :)
    charbuffer[buffer.size()] = 0x00;
    std::vector<std::string> lines = StringUtils::Split(charbuffer, '\n');
    for (auto line: lines)
    {
      std::vector<std::string> items = StringUtils::Split(line, ',');
      TVAPI_File downloadedFile;
      switch(items.size())
      {
        case 3:
          downloadedFile.format = items[2];
          // no break so drop through to next
        case 2:
          downloadedFile.date = items[0];
          downloadedFile.id = items[1];
          downloadedFiles.files.push_back(downloadedFile);
          break;
        default:
          if (!line.empty() && line[0] != 0x00)
            CLog::Log(LOGDEBUG, "**NW** - SendFilesDownloaded, split failed = %s", line.c_str());
          break;
      }
    }

    if (!downloadedFiles.files.empty())
    {
      if (TVAPI_ReportFilesDownloaded(downloadedFiles))
        XFILE::CFile::Delete(filename);
    }
  }
}

void CNWClient::LogFilesDownLoaded(std::string assetID,std::string assetFormat)
{
  if (!m_HasNetwork)
    return;

  //  date,assetID,type
  //  2015-02-05 12:01:40-0500,58350,4K
  //  2015-02-05 12:05:40-0500,57116,720

  CSingleLock lock(m_reportLock);
  CDateTime time = CDateTime::GetCurrentDateTime();
  std::string strData = StringUtils::Format("%s,%s,%s\n",
    time.GetAsDBDateTime().c_str(),
    assetID.c_str(),
    assetFormat.c_str()
  );

  XFILE::CFile file;
  std::string filename = m_strHome + "log/" + m_PlayerInfo.id + "_download.log";
  file.OpenForWrite(filename);
  file.Seek(0, SEEK_END);
  file.Write(strData.c_str(), strData.size());
  file.Close();

  // send it to IoT as well
  CVariant data(CVariant::VariantTypeObject);
  data["end"] = false;

  data["assetID"] = assetID;
  data["format"] = assetFormat;
  data["payload"] = strData;
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Player, "MNassetDownloaded", data);
}

void CNWClient::UpdateNetworkStatus()
{
  m_HasNetwork = HasInternet();
  m_MediaManager->UpdateNetworkStatus(m_HasNetwork);
}

bool CNWClient::SendPlayerStatus(const std::string status)
{
  m_PlayerInfo.status = status;

  if (!m_HasNetwork)
    return true;

  TVAPI_MachineUpdate machineUpdate;
  machineUpdate.apiKey = m_PlayerInfo.apiKey;
  machineUpdate.apiSecret = m_PlayerInfo.apiSecret;
  machineUpdate.playlist_id = m_PlayerInfo.id;
  machineUpdate.name = m_PlayerInfo.name;
  machineUpdate.description = m_PlayerInfo.description;
  machineUpdate.serial_number = m_PlayerInfo.serial_number;
  machineUpdate.warranty_number = m_PlayerInfo.warranty_number;
  machineUpdate.macaddress = m_PlayerInfo.macaddress;
  machineUpdate.macaddress_wireless = m_PlayerInfo.macaddress_wireless;
  machineUpdate.vendor = m_PlayerInfo.vendor;
  machineUpdate.hardware_version = m_PlayerInfo.hardware_version;
  machineUpdate.timezone = m_PlayerInfo.timezone;
  machineUpdate.status = m_PlayerInfo.status;
  machineUpdate.allow_new_content = m_PlayerInfo.allow_new_content;
  machineUpdate.allow_software_update = m_PlayerInfo.allow_software_update;
  machineUpdate.update_interval = m_PlayerInfo.update_interval;
  machineUpdate.update_time = m_PlayerInfo.update_time;
  return TVAPI_UpdateMachineInfo(machineUpdate);
}

void CNWClient::InitializeInternalsFromPlayer()
{
  if (!m_PlayerInfo.update_time.empty() && !m_PlayerInfo.update_interval.empty())
  {
    // "update_interval":"86400" or "daily", "update_time":"24:00"
    CDateTime time = CDateTime::GetCurrentDateTime();
    if (m_PlayerInfo.update_interval == "daily")
    {
      int hours, minutes;
      sscanf(m_PlayerInfo.update_time.c_str(),"%d:%d", &hours, &minutes);
      m_NextUpdateTime.SetDateTime(time.GetYear(), time.GetMonth(), time.GetDay(), hours, minutes, 0);

      // next update is 24 hours from m_NextUpdateTime
      m_NextUpdateInterval.SetDateTimeSpan(0, 24, 0, 0);
    }
    else
    {
      // if update_interval is not "daily", it is set to minutes
      int update_interval = atoi(m_PlayerInfo.update_interval.c_str());
      // we add minutes to current time to trigger the next update
      m_NextUpdateTime = time + CDateTimeSpan(0, 0, update_interval, 0);
      m_NextUpdateInterval.SetDateTimeSpan(0, 0, update_interval, 0);
    }

    // make sure we schedual next update past the current time
    if (time >= m_NextUpdateTime)
      m_NextUpdateTime += m_NextUpdateInterval;

    #if ENABLE_NWCLIENT_DEBUGLOGS
    CLog::Log(LOGDEBUG, "**NW** - (from player) m_NextUpdateTime = %s", m_NextUpdateTime.GetAsDBDateTime().c_str());
    CLog::Log(LOGDEBUG, "**NW** - (from player) m_NextUpdateInterval = %d days, %d hours, %d mins",
      m_NextUpdateInterval.GetDays(), m_NextUpdateInterval.GetHours(), m_NextUpdateInterval.GetMinutes());
    #endif

  }

  if (!m_PlayerInfo.tvapiURLBase.empty())
    TVAPI_SetURLBASE(m_PlayerInfo.tvapiURLBase);
}


static std::string CheckForVideoFormatAndFallBack(const std::string &video_format, const std::vector<TVAPI_PlaylistFile> &files)
{
  // if there is a match, return it
  for (auto file : files)
  {
    if (file.type == video_format)
      return video_format;
  }

  // if 4k, check for 1080, then 720
  if (video_format == "4K")
  {
    for (auto file : files)
    {
      if (file.type == "1080")
        return file.type;
    }
    for (auto file : files)
    {
      if (file.type == "720")
        return file.type;
    }
  }

  // if 1080, check for 720
  if (video_format == "1080")
  {
    for (auto file : files)
    {
      if (file.type == "720")
        return file.type;
    }
  }

  return "not found";
}

bool CNWClient::CreatePlaylist(std::string home, NWPlaylist &playList,
  const TVAPI_Playlist &playlist, const TVAPI_PlaylistItems &playlistItems)
{
  // convert server structures to player structure
  playList.id = std_stoi(playlist.id);
  playList.name = playlist.name;
  playList.type = playlist.type;
  // format is "2013-08-22"
  playList.layout = playlist.layout;
  playList.updated_date = playlist.updated_date;
  playList.groups.clear();
  playList.play_order.clear();

  if (!playlist.categories.empty())
  {
    for (auto catagory : playlist.categories)
    {
      NWGroup group;
      group.id = std_stoi(catagory.id);
      group.name = catagory.name;
      group.next_asset_index = 0;
      // always remember original group play order
      // this determines group play order. ie.
      // pick 1st group, play asset, pick next group, play asset, do all groups
      // cycle back, pick 1st group, play next asset...
      playList.play_order.push_back(group.id);

      // check if we already have handled this group
      auto it = std::find_if(playList.groups.begin(), playList.groups.end(),
        [group](const NWGroup &existingGroup) { return existingGroup.id == group.id; });
      if (it == playList.groups.end())
      {
        // not present, pull out assets assigned to this group
        for (auto item : playlistItems.items)
        {
          if (item.tv_category_id == catagory.id)
          {
            NWAsset asset;
            asset.id = std_stoi(item.id);
            asset.name = item.name;
            asset.group_id = std_stoi(item.tv_category_id);
            asset.valid = false;

            std::string video_format = CheckForVideoFormatAndFallBack(playList.video_format, item.files);
            for (auto file : item.files)
            {
              if (file.type == video_format)
              {
                // trap out bad urls
                if (file.path.find("proxy.membernettv.com") != std::string::npos)
                  continue;
                asset.id = std_stoi(item.id);
                asset.type = file.type;
                asset.video_url = file.path;
                asset.video_md5 = file.etag;
                asset.video_size = std_stoi(file.size);
                // format is "2013-02-27 01:00:00"
                asset.available_to.SetFromDBDateTime(item.availability_to);
                asset.available_from.SetFromDBDateTime(item.availability_from);
                asset.video_basename = URIUtils::GetFileName(asset.video_url);
                std::string video_extension = URIUtils::GetExtension(asset.video_url);
                std::string localpath = kNWClient_DownloadVideoPath + std_to_string(asset.id) + video_extension;
                asset.video_localpath = URIUtils::AddFileToFolder(home, localpath);
                break;
              }
            }
            // if we got an complete asset, save it.
            if (!asset.video_url.empty())
            {
              // trap out bad urls
              if (item.thumb.path.find("proxy.membernettv.com") == std::string::npos)
              {
                // bring over thumb references
                asset.thumb_url = item.thumb.path;
                asset.thumb_md5 = item.thumb.etag;
                asset.thumb_size = std_stoi(item.thumb.size);
                asset.thumb_basename = URIUtils::GetFileName(asset.thumb_url);
                std::string thumb_extension = URIUtils::GetExtension(asset.thumb_url);
                std::string localpath = kNWClient_DownloadVideoThumbNailsPath + std_to_string(asset.id) + thumb_extension;
                asset.thumb_localpath = URIUtils::AddFileToFolder(home, localpath);
              }
              group.assets.push_back(asset);
            }
          }
        }
        // add the new group regardless of it there are assets present
        // player will skip over this group if there are no assets.
        playList.groups.push_back(group);
      }
    }
  }
  else if (!playlist.files.empty())
  {
    NWGroup group;
    group.id = playList.id;
    group.name = playList.name;
    group.next_asset_index = 0;
    // always remember original group play order
    // this determines group play order. ie.
    // pick 1st group, play asset, pick next group, play asset, do all groups
    // cycle back, pick 1st group, play next asset...
    playList.play_order.push_back(group.id);

    for (auto id : playlist.files)
    {
      // find the file item that matches this id
      auto it = std::find_if(playlistItems.items.begin(), playlistItems.items.end(),
        [id](const TVAPI_PlaylistItem &item) { return item.id == id.id; });
      if (it != playlistItems.items.end())
      {
        const TVAPI_PlaylistItem &item = *it;
        NWAsset asset;
        asset.id = std_stoi(item.id);
        asset.name = item.name;
        asset.group_id = group.id;
        asset.valid = false;

        std::string video_format = CheckForVideoFormatAndFallBack(playList.video_format, item.files);
        for (auto file : item.files)
        {
          if (file.type == video_format)
          {
            // trap out bad urls
            if (file.path.find("proxy.membernettv.com") != std::string::npos)
              continue;
            asset.id = std_stoi(item.id);
            asset.type = file.type;
            asset.video_url = file.path;
            asset.video_md5 = file.etag;
            asset.video_size = std_stoi(file.size);
            // format is "2013-02-27 01:00:00"
            asset.available_to.SetFromDBDateTime(item.availability_to);
            asset.available_from.SetFromDBDateTime(item.availability_from);
            asset.video_basename = URIUtils::GetFileName(asset.video_url);
            std::string video_extension = URIUtils::GetExtension(asset.video_url);
            std::string localpath = kNWClient_DownloadVideoPath + std_to_string(asset.id) + video_extension;
            asset.video_localpath = URIUtils::AddFileToFolder(home, localpath);
            break;
          }
        }
        // if we got an complete asset, save it.
        if (!asset.video_url.empty())
        {
          // trap out bad urls
          if (item.thumb.path.find("proxy.membernettv.com") == std::string::npos)
          {
            // bring over thumb references
            asset.thumb_url = item.thumb.path;
            asset.thumb_md5 = item.thumb.etag;
            asset.thumb_size = std_stoi(item.thumb.size);
            asset.thumb_basename = URIUtils::GetFileName(asset.thumb_url);
            std::string thumb_extension = URIUtils::GetExtension(asset.thumb_url);
            std::string localpath = kNWClient_DownloadVideoThumbNailsPath + std_to_string(asset.id) + thumb_extension;
            asset.thumb_localpath = URIUtils::AddFileToFolder(home, localpath);
          }
          group.assets.push_back(asset);
        }
      }
    }
    // add the new group regardless of it there are assets present
    // player will skip over this group if there are no assets.
    playList.groups.push_back(group);
  }

  return false;
}

void CNWClient::AssetUpdateCallBack(const void *ctx, NWAsset &asset, AssetDownloadState downloadState)
{
  CNWClient *client = (CNWClient*)ctx;

  if (downloadState == AssetDownloadState::IsPresent)
    client->m_Player->MarkValidated(asset);

  if (downloadState == AssetDownloadState::wasDownloaded)
  {
    client->m_Player->MarkValidated(asset);
    client->LogFilesDownLoaded(std_to_string(asset.id),asset.type);
  }

  if (client->m_Player->IsPlaying() || !client->m_bypassDownloadWait)
  {
    if (client->m_Startup && client->m_dlgProgress->IsDialogRunning())
    {
      int assetcount = client->m_MediaManager->GetLocalAssetCount();
      if (downloadState == AssetDownloadState::willDownload)
        client->m_dlgProgress->SetLine(1, StringUtils::Format("Downloading: %s", asset.name.c_str()));
      else
        client->m_dlgProgress->SetLine(1, StringUtils::Format("Checking: %s", asset.name.c_str()));
      client->m_dlgProgress->SetLine(2, StringUtils::Format("Asset %i/%i", assetcount, client->m_totalAssets));
      client->m_dlgProgress->SetPercentage(int(float(assetcount) / float(client->m_totalAssets) * 100));
    }
  }
}

bool CNWClient::DoAuthorize()
{
/*
  m_activate.code = "HPPALSRK/A";
  if (!TVAPI_DoActivate(m_activate))
  {
    m_activate.apiKey = "gMFQKKYS/Ib3Kyo/2oMA";
    m_activate.apiSecret = "HtqhPrk3JyvX5bDSay75OY1RHTvGAhxwg51Kh7KJ";
  }
*/
  if (!HasInternet())
    return !m_PlayerInfo.apiKey.empty() && !m_PlayerInfo.apiSecret.empty();

  std::string code = "";
  const std::string header = "Enter Activation Code";

  if (CGUIKeyboardFactory::ShowAndGetInput(code, CVariant{header}, false))
  {
    size_t test_url_pos = code.find("test://");
    if (test_url_pos == std::string::npos)
      test_url_pos = code.find("TEST://");

    if (test_url_pos != std::string::npos)
    {
      // if we find code starts with test site url, switch to test site
      TVAPI_SetURLBASE(kTVAPI_URLBASE_TESTSITE);
      code.erase(test_url_pos, strlen("TEST://"));
    }
    else
    {
      TVAPI_SetURLBASE(kTVAPI_URLBASE);
    }

    TVAPI_Activate activate;
    activate.code = code;
#if defined(TARGET_ANDROID)
    activate.application_id = jni::CJNISettingsSecure::getString(CJNIContext::getContentResolver(), jni::CJNISettingsSecure::ANDROID_ID);
#else
//    activate.application_id = CDarwinUtils::GetHardwareUUID();
#endif

    // davilla, check if this is okay?
    activate.application_id = StringUtils::CreateUUID();

    if (code.find("NWMNDEMO4K") != std::string::npos)
    {
      m_PlayerInfo.apiKey = "wAE/V6Gq3X3h0ZOjcK/A";
      m_PlayerInfo.apiSecret = "dMLudiHXRKc18ZJXFk4o7pyhaSww41/kvnjbmc4L";
      TVAPI_SetURLBASE(kTVAPI_URLBASE);
      SaveLocalPlayer(m_strHome, m_PlayerInfo);
      return true;
    }
    else if (code.find("NWMNDEMO") != std::string::npos)
    {
      // special case for code of 'NWMNDEMO'
      m_PlayerInfo.apiKey = "LbpCC91TBDsoHExRxvtV";
      m_PlayerInfo.apiSecret = "RN16RS1PUZVt8xgW+URBjU0o/ZXcdLWUDA45v2qQ";
      TVAPI_SetURLBASE(kTVAPI_URLBASE_TESTSITE);
      SaveLocalPlayer(m_strHome, m_PlayerInfo);
      return true;
    }
    else
    {
      if (TVAPI_DoActivate(activate))
      {
        m_PlayerInfo.apiKey = activate.apiKey;
        m_PlayerInfo.apiSecret = activate.apiSecret;
        TVAPI_Status  status;
        status.apiKey = m_PlayerInfo.apiKey;
        status.apiSecret = m_PlayerInfo.apiSecret;
        SaveLocalPlayer(m_strHome, m_PlayerInfo);
        return TVAPI_GetStatus(status);
      }
    }
  }

  CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, "Activation Failed/Cancelled", "", 4000, false);
  return false;
}

bool CNWClient::IsAuthorized()
{
  if (!HasInternet())
    return !m_PlayerInfo.apiKey.empty() && !m_PlayerInfo.apiSecret.empty();

  if (!m_PlayerInfo.apiKey.empty() && !m_PlayerInfo.apiSecret.empty())
  {
    TVAPI_Status  status;
    status.apiKey = m_PlayerInfo.apiKey;
    status.apiSecret = m_PlayerInfo.apiSecret;
    return TVAPI_GetStatus(status);
  }
  else
    return false;
}

bool CNWClient::AllowExit()
{
  // we dont allow exit in MN v3
  return false;
  // only block exit if we are authorized
  // and not using NWMNDEMO/NWMNDEMO4K codes
//  if (IsAuthorized())
//  {
//    // NWMNDEMO4K is allowed to escape
//    if (m_PlayerInfo.apiKey.find("wAE/V6Gq3X3h0ZOjcK/A") != std::string::npos)
//      return true;
//    // NWMNDEMO is allowed to escape
//    if (m_PlayerInfo.apiKey.find("LbpCC91TBDsoHExRxvtV") != std::string::npos)
//      return true;
//
//    return false;
//  }
//  return true;
}

bool CNWClient::CheckUpdate()
{
  m_updaterRunning = true;
  std::string updateURL = "https://mrmc.tv/mrmc/build-deps/mnUpdate.json";
  std::string updateFolder;
  std::string tempPath = CSpecialProtocol::TranslatePath("special://temp/");
#ifndef TARGET_DARWIN
  updateFolder = "/storage/.update/";
#else
  updateFolder = CSpecialProtocol::TranslatePath("special://nwmn/downloads/");
#endif

  CURL url(updateURL);
  XFILE::CCurlFile curl;
  CVariant reply;
  std::string strResponse;
  std::string appVersion = CSysInfo::GetVersionShort();
  if (curl.Get(url.Get(), strResponse))
  {
    if (!CJSONVariantParser::Parse(strResponse, reply))
      return false;

    std::string tarUrl         = reply["update"]["updateUrl"].asString();
    std::string tarMD5         = reply["update"]["updateMd5"].asString();
    unsigned int tarsize       = reply["update"]["updateSize"].asInteger();
    std::string updateVersion  = reply["update"]["version"].asString();
    bool        forceUpdate    = reply["update"]["forceUpdate"].asBoolean();

    if (forceUpdate || LessThanVersion(appVersion, updateVersion))
    {
      m_dlgProgress->SetLine(0, StringUtils::Format("Downloading firmware update"));
      m_dlgProgress->SetLine(1, StringUtils::Format("Please wait..."));
      CLog::Log(LOGINFO, "**NW** - CNWClient::CheckUpdate() Update needed, Current OS version '%s' - update version '%s', forceUpdate = %s",appVersion, updateVersion, forceUpdate ? "true":"false");
      // run update
      std::string tarTempLocal = URIUtils::AddFileToFolder(tempPath, URIUtils::GetFileName(tarUrl));
      if (XFILE::CFile::Exists(tarTempLocal))
        XFILE::CFile::Delete(tarTempLocal);

      XFILE::CCurlFile http;
      http.Download(tarUrl, tarTempLocal, &tarsize);
      std::string localTarMD5 = CUtil::GetFileDigest(tarTempLocal, KODI::UTILITY::CDigest::Type::MD5);
      CLog::Log(LOGINFO, "**NW** - CNWClient::CheckUpdate() downloaded update tar MD5(%s)", localTarMD5);

      if (StringUtils::EqualsNoCase(tarMD5, localTarMD5))
      {
        //
        std::string tarLocal = URIUtils::AddFileToFolder(updateFolder, URIUtils::GetFileName(tarTempLocal));
        XFILE::CFile::Rename(tarTempLocal, tarLocal);
        CLog::Log(LOGINFO, "**NW** - CNWClient::CheckUpdate() Rebooting...");
#ifndef TARGET_DARWIN
        system("/usr/bin/systemctl --no-block reboot");
#endif
      }
    }

  }
  m_updaterRunning = false;
  return true;
}
