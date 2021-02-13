#pragma once

/*
 *  Copyright (C) 2020 RootCoder, LLC.
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

#include "NWClientUtilities.h"

#include "interfaces/IAnnouncer.h"
#include "threads/CriticalSection.h"
#include "threads/Thread.h"
#include "utils/Stopwatch.h"

#define ENABLE_NWIOT_DEBUGLOGS 1

// ---------------------------------------------
// ---------------------------------------------

class CNWIoT : public ANNOUNCEMENT::IAnnouncer, public CThread
{
public:
  CNWIoT();
  ~CNWIoT();

  void Announce(ANNOUNCEMENT::AnnouncementFlag flag,
                const std::string& sender,
                const std::string& message,
                const CVariant& data) override;
  void   MsgReceived(CVariant msgPayload);
  bool   DoAuthorize();
  bool   IsAuthorized();
  void   Listen();
  void   setPayload(std::string payload = "");
  void   notifyEvent(std::string type, CVariant details);

protected:
  void Process() override;
  static CCriticalSection m_payloadLock;
  std::string   m_payload;
  CStopWatch m_heartbeatTimer;

};

