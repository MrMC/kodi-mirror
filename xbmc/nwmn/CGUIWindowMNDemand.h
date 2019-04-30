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

#include "NWClient.h"

#include "cores/IPlayerCallback.h"
#include "guilib/GUIWindow.h"

class CGUIWindowMNDemand : public CGUIWindow
{
public:
  CGUIWindowMNDemand();
  virtual ~CGUIWindowMNDemand();

  virtual bool  OnMessage(CGUIMessage& message);
  virtual void  OnInitWindow();
  virtual void  OnWindowUnload();
  virtual bool  OnAction(const CAction& action);
  void          FillAssets();
  void          SetControlLabel(int id, const char *format, int info);
  static  CGUIWindowMNDemand* GetDialogMNDemand();
  static void   GetDialogMNPlaylist(NWPlaylist &playList);
  static void   SetDialogMNPlaylist(const NWPlaylist &playList);
  static void   SetClient(CNWClient *client) {m_client = client;};

protected:
  void SetCategoryItems();

  static CGUIWindowMNDemand *m_MNDemand;
  static NWPlaylist m_PlayList;
  static CNWClient  *m_client;
};
