#pragma once

/*
 *  Copyright (C) 2017 RootCoder, LLC.
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
#include <vector>

#include "threads/Thread.h"
#include "threads/CriticalSection.h"

#define ENABLE_NWMPURGEMANAGER_DEBUGLOGS 1

class CNWPurgeManager : public CThread
{
public:
  CNWPurgeManager();
  virtual ~CNWPurgeManager();

  void AddPurgePath(const std::string &media, const std::string &thumb);

protected:
  virtual void  Process();
  void          UpdateMargins();
  bool          PurgeLastAccessed();

  int m_freeMBs;
  int m_loFreeMBs;
  int m_hiFreeMBs;
  bool m_canPurge;
  CEvent m_processSleep;
  CCriticalSection m_paths_lock;
  std::vector<std::string> m_media_paths;
  std::vector<std::string> m_thumb_paths;
};
