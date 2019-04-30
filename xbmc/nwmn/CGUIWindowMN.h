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


#include <string>

#include "cores/IPlayerCallback.h"
#include "guilib/GUIWindow.h"
#include "threads/Thread.h"

class  CNWClient;
struct NWAsset;

class CGUIWindowMN : public CGUIWindow, public CThread
{
public:
  CGUIWindowMN();
  virtual ~CGUIWindowMN();

  virtual bool  OnMessage(CGUIMessage& message);
  virtual void  OnInitWindow();
  virtual void  OnWindowLoaded();
  virtual void  OnWindowUnload();

  void          Refresh(bool fetchAndUpdate);
  void          DisableButtonsOnRefresh(bool disable);

protected:
  virtual void  Process(unsigned int currentTime, CDirtyRegionList &dirtyregions);
  static  void  ClientCallBack(const void *ctx, int msg);
  static  void  PlayerCallBack(const void *ctx, int msg, struct NWAsset &asset);
  virtual bool  OnAction(const CAction &action);
  virtual void  Process();
  virtual void  OnStartup();
  void          TestServers();
  bool          PingMNServer(const std::string& strURL);
  void          SetResolution(const std::string &strResolution);
  void          StartClient(bool force);

  bool          m_RefreshRunning;
  bool          m_AboutUp;
  bool          m_testServersPopup;
  bool          m_testServers;
  CNWClient    *m_client;
};
