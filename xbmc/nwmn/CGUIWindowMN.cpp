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

#include "CGUIWindowMN.h"

#include "CGUIWindowMNDemand.h"
#include "NWClient.h"
#include "NWTVAPI.h"

#include "Application.h"
#include "URL.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "filesystem/CurlFile.h"
#include "input/Key.h"
#include "network/Network.h"
#include "settings/DisplaySettings.h"
#include "settings/SkinSettings.h"
#include "settings/Settings.h"
#include "messaging/ApplicationMessenger.h"
//#include "utils/Environment.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "settings/SettingsComponent.h"
#include "guilib/GUIComponent.h"
#include "interfaces/AnnouncementManager.h"



#define PLAYLIST          90101
#define ONDEMAND          90102
#define MEDIAUPDATE       90103
#define ABOUT             90105
#define NETWORKSET        90116
#define NETWORKTEST       90126
#define SETVERTICAL       90134
#define SETHORIZONTAL     90144
#define AUTHORIZE         90150
#define ABOUTDIALOG       90200

#define PINGDIALOG        90145
#define GOOGLESERVER      90147
#define NWSEREVER         90148
#define PROXYMNSERVER     90149

#define STARTCLIENT       90999



CGUIWindowMN::CGUIWindowMN()
: CGUIWindow(WINDOW_MEMBERNET, "DialogNationWide.xml")
, CThread("MNwindow")
, m_RefreshRunning(false)
, m_AboutUp(false)
, m_testServersPopup(false)
, m_testServers(false)
, m_client(NULL)
{
  Create();
  m_loadType = KEEP_IN_MEMORY;
}

CGUIWindowMN::~CGUIWindowMN()
{
  StopThread();
  Close();
}

bool CGUIWindowMN::OnMessage(CGUIMessage& message)
{
  if  (message.GetMessage() == GUI_MSG_CLICKED)
  {
    int iControl = message.GetSenderId();
    
    if (iControl == ABOUT && m_client)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), ABOUT);
      OnMessage(msg);
      
      m_AboutUp = true;
      SET_CONTROL_VISIBLE(ABOUTDIALOG);
      
      std::string strIPAddress;
      CNetworkInterface* iface = CServiceBroker::GetNetwork().GetFirstConnectedInterface();
      if (iface)
        strIPAddress = iface->GetCurrentIPAddress();

      NWPlayerInfo playerInfo;
      m_client->GetPlayerInfo(playerInfo);
      // Fill in about popup
      SET_CONTROL_LABEL(90212, StringUtils::Format("Machine Name: %s",
                                                   playerInfo.name.c_str()));
      SET_CONTROL_LABEL(90213, StringUtils::Format("Serial Number: %s",
                                                   playerInfo.serial_number.c_str()));
//      SET_CONTROL_LABEL(90212, StringUtils::Format("Location ID: %s",
//                                                   playerInfo..c_str()));
//      SET_CONTROL_LABEL(90213, StringUtils::Format("Machine ID: %s",
//                                                   settings.strMachine_id.c_str()));
      SET_CONTROL_LABEL(90214, StringUtils::Format("MNTV Software Version: %s",
                                                   playerInfo.software_version.c_str()));
      SET_CONTROL_LABEL(90215, StringUtils::Format("IP Address:  %s",
                                                   strIPAddress.c_str()));
      SET_CONTROL_LABEL(90216, StringUtils::Format("Ethernet MAC: %s",
                                                   playerInfo.macaddress.c_str()));
      SET_CONTROL_LABEL(90217, StringUtils::Format("Wireless MAC: %s",
                                                   playerInfo.macaddress_wireless.c_str()));
      SET_CONTROL_LABEL(90218, StringUtils::Format("Free Space: %s/%s",
                                                   GetDiskFree("/").c_str(),GetDiskTotal("/").c_str()));
      SET_CONTROL_LABEL(90219, StringUtils::Format("System Uptime: %s",
                                                   GetSystemUpTime().c_str()));
      CVariant data;
      data["msg"] = "about";
      std::string payload;
      payload = playerInfo.macaddress + "\n";
      payload += playerInfo.serial_number + "\n";
      data["payload"] = payload;
      CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Other, "xbmc", "MNmsg", data);
      return true;
    }
    else if (iControl == PLAYLIST && m_client)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), PLAYLIST);
      OnMessage(msg);
      if (m_client->IsAuthorized())
        Refresh(false);
      return true;
    }
    else if (iControl == MEDIAUPDATE && m_client)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), MEDIAUPDATE);
      OnMessage(msg);
      if (m_client->IsAuthorized())
        Refresh(true);
      return true;
    }
    else if (iControl == NETWORKTEST)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), NETWORKTEST);
      OnMessage(msg);
      m_testServers = true;
      return true;
    }
    else if (iControl == SETVERTICAL)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), SETVERTICAL);
      OnMessage(msg);
      CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::MN_VERTICAL, true);
      CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
      SET_CONTROL_FOCUS(SETHORIZONTAL,0);
      return true;
    }
    else if (iControl == SETHORIZONTAL)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), SETHORIZONTAL);
      OnMessage(msg);
      CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::MN_VERTICAL, false);
      CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
      SET_CONTROL_FOCUS(SETVERTICAL,0);
      return true;
    }
    else if (iControl == ONDEMAND && m_client)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), ONDEMAND);
      OnMessage(msg);
      
      //fill in on demand window here
      NWPlaylist playList;
      m_client->GetProgamInfo(playList);
      if (!playList.groups.empty())
      {
        CGUIWindowMNDemand::SetClient(m_client);
        CGUIWindowMNDemand::SetDialogMNPlaylist(playList);
        CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_MEMBERNET_DEMAND);
        m_client->StopPlaying();
      }
      return true;
    }
    else if (iControl == AUTHORIZE)
    {
      CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), AUTHORIZE);
      OnMessage(msg);
      
      CVariant data;
      data["msg"] = "authorise";
      CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Other, "xbmc", "MNmsg", data);
      return true;
    }
  }
  else if (message.GetMessage() == GUI_MSG_WINDOW_DEINIT)
  {
    // below prevents window from exiting into home
    // CGUIWindow::OnMessage(message);
    return true;
  }
  else if (message.GetMessage() == GUI_MSG_NOTIFY_ALL)
  {
    if (message.GetParam1() == STARTCLIENT)
      StartClient(message.GetSenderId() != GetID());
  }
  return CGUIWindow::OnMessage(message);
}


bool CGUIWindowMN::OnAction(const CAction &action)
{

  if (action.GetButtonCode() == KEY_BUTTON_BACK || action.GetID() == ACTION_PREVIOUS_MENU || action.GetID() == ACTION_NAV_BACK)
  {
    
    if (m_AboutUp)
    {
      SET_CONTROL_HIDDEN(ABOUTDIALOG);
      m_AboutUp = false;
      return true;
    }

    if (m_testServersPopup)
    {
      SET_CONTROL_HIDDEN(PINGDIALOG);
      m_testServersPopup = false;
      //return true;
    }
    
    if (m_client && g_application.GetAppPlayer().IsPlaying())
    {
      m_client->StopPlaying();
      return true;
    }
    if (m_client && m_client->AllowExit())
      KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
    // this return blocks any other "back/esc" action, prevents us closing MN main screen
    return true;
  }
  
  return CGUIWindow::OnAction(action);

}

void CGUIWindowMN::OnInitWindow()
{
  CGUIMessage reload(GUI_MSG_NOTIFY_ALL, GetID(), 0, STARTCLIENT, 0);
  CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(reload, GetID());
  
  CGUIWindow::OnInitWindow();
}

void CGUIWindowMN::OnWindowLoaded()
{
  CGUIWindow::OnWindowLoaded();
}

void CGUIWindowMN::OnWindowUnload()
{
  SAFE_DELETE(m_client);
}

void CGUIWindowMN::Refresh(bool fetchAndUpdate)
{
  CLog::Log(LOGDEBUG, "**NW** - CGUIWindowMN::Refresh()");
  if (!m_RefreshRunning)
  {
    m_RefreshRunning = true;
    if (m_client)
      m_client->Startup(false, fetchAndUpdate);
  }
}

void CGUIWindowMN::StartClient(bool force)
{
  if (!m_client)
  {
    m_client = new CNWClient();
    m_client->RegisterClientCallBack(this, ClientCallBack);
    m_client->RegisterPlayerCallBack(this, PlayerCallBack);
    m_client->Startup(false, false);
  }
  else
  {
    if (force)
      m_client->Startup(true, true);
  }

}

void CGUIWindowMN::OnStartup()
{
  
}

void  CGUIWindowMN::Process()
{
  while (!m_bStop)
  {
    Sleep(500);
    if (m_testServers)
    {
      m_testServers = false;
      m_testServersPopup = true;
      SET_CONTROL_VISIBLE(PINGDIALOG);
      TestServers();
    }
  }
}

void CGUIWindowMN::Process(unsigned int currentTime, CDirtyRegionList &dirtyregions)
{
  CGUIWindow::Process(currentTime, dirtyregions);
}

void CGUIWindowMN::ClientCallBack(const void *ctx, int msg)
{
  CLog::Log(LOGDEBUG, "**NW** - CGUIWindowMN::ClientCallBack() msg = %d", msg);
  CGUIWindowMN *dlog = (CGUIWindowMN*)ctx;
  switch(msg)
  {
    case 0:
      break;
    case 1:
      break;
    case 2:
      break;
  }
  dlog->m_RefreshRunning = false;
}

void CGUIWindowMN::PlayerCallBack(const void *ctx, int msg, NWAsset &asset)
{
  CLog::Log(LOGDEBUG, "**NW** - CGUIWindowMN::PlayerCallBack(): playing \'%s\'", asset.name.c_str());
}

void CGUIWindowMN::DisableButtonsOnRefresh(bool disable)
{
  if (disable)
  {
    CONTROL_DISABLE(PLAYLIST);
    CONTROL_DISABLE(ONDEMAND);
    CONTROL_DISABLE(MEDIAUPDATE);
  }
  else
  {
    CONTROL_ENABLE(PLAYLIST);
    CONTROL_ENABLE(ONDEMAND);
    CONTROL_ENABLE(MEDIAUPDATE);
  }
}


void CGUIWindowMN::TestServers()
{
  SET_CONTROL_LABEL(GOOGLESERVER, "'www.google.com' --> testing");
  SET_CONTROL_LABEL(NWSEREVER, "'www.nationwidemember.com' --> testing");
  SET_CONTROL_LABEL(PROXYMNSERVER, "");
  
  if (PingMNServer("http://www.google.com"))
    SET_CONTROL_LABEL(GOOGLESERVER, "'www.google.com' is reachable");
  else
    SET_CONTROL_LABEL(GOOGLESERVER, "'www.google.com' is not reachable");
  
  if (PingMNServer("http://www.nationwidemember.com"))
    SET_CONTROL_LABEL(NWSEREVER, "'www.nationwidemember.com' is reachable");
  else
    SET_CONTROL_LABEL(NWSEREVER, "'www.nationwidemember.com' is not reachable");
  /*
  if (PingMNServer("http://proxy.membernettv.com"))
    SET_CONTROL_LABEL(PROXYMNSERVER, "'proxy.membernettv.com' is reachable");
  else
    SET_CONTROL_LABEL(PROXYMNSERVER, "'proxy.membernettv.com' is not reachable");
  */
}

bool CGUIWindowMN::PingMNServer(const std::string& apiURL)
{
  CURL url(apiURL.c_str());
  std::string http_path = url.GetProtocol().c_str();
  http_path += "://" + url.GetHostName();

  XFILE::CCurlFile http;
  CURL http_url(http_path.c_str());
  bool found = http.Open(http_url);
  if (!found && (errno == EACCES))
    found = true;
  http.Close();
  
  if (found)
    CLog::Log(LOGDEBUG, "**MN** - PingMNServer: network=yes");
  else
    CLog::Log(LOGDEBUG, "**MN** - PingMNServer: network=no");

  return found;
}

void CGUIWindowMN::SetResolution(const std::string &strResolution)
{
  // format: SWWWWWHHHHHRRR.RRRRRP, where,
  //  S = screen, W = width, H = height, R = refresh, P = interlace
  RESOLUTION res = CDisplaySettings::GetResolutionFromString(strResolution);
//  const char *strMode = CDisplaySettings::Get().GetResolutionInfo(res).strMode.c_str();
  CDisplaySettings::GetInstance().SetCurrentResolution(res, true);
  CLog::Log(LOGDEBUG,"resolution:res(%d), parameter(%s)", res, strResolution.c_str());
}
