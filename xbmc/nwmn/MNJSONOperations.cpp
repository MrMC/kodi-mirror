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

#include "MNJSONOperations.h"

#include "NWClient.h"

#include "utils/Variant.h"
#include "interfaces/json-rpc/JSONRPCUtils.h"
#include "utils/JSONVariantWriter.h"
#include "utils/JSONVariantParser.h"

#include "filesystem/File.h"
#include "utils/log.h"
#include "utils/SystemInfo.h"
#include "settings/Settings.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "filesystem/SpecialProtocol.h"

using namespace JSONRPC;

JSONRPC_STATUS CMNJSONOperations::SetPlayerSettings(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  std::string argv;
  CVariant params = parameterObject["params"];
  
  if (params.isObject())
  {
    for (CVariant::const_iterator_map it = params.begin_map(); it != params.end_map(); it++)
    {
      if (it->first == "values")
      {
        argv = it->second.asString();
        StringUtils::Replace(argv, ";", ",");
      }
    }
  }
  
  std::string settingsFile = "special://home/MN/settings.txt";
  
  XFILE::CFile file;
  if (!file.OpenForWrite(settingsFile, true))
  {
    CLog::Log(LOGERROR, "MN Save settings.txt - Unable to open file");
  }
  else
  {
    file.Write(argv.c_str(), argv.size());
  }
  file.Close();
  
  CVariant obj;
  CJSONVariantParser::Parse(argv, obj);
  
  std::string url;
  std::string machineID;
  std::string locationID;
  
  if (obj.isObject())
  {
    for (CVariant::const_iterator_map it = obj.begin_map(); it != obj.end_map(); it++)
    {
      if (it->first == "url")
      {
        url = it->second["feed"].asString();
      }
      else if (it->first == "location")
      {
        locationID = it->second["id"].asString();
      }
      else if (it->first == "machine")
      {
        machineID = it->second["id"].asString();
      }
    }
  }
  
  if (url.empty() || machineID.empty() || locationID.empty())
  {
    CLog::Log(LOGERROR, "MN ERROR parsing settings, url - %s, Machine ID - %s , location ID - %s", url.c_str(), machineID.c_str(), locationID.c_str());
  }
  else
  {
    CLog::Log(LOGDEBUG, "MN Updated settings, url - %s, Machine ID - %s , location ID - %s", url.c_str(), machineID.c_str(), locationID.c_str());
    
    // Notify that we have changed settings
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info,
                                          "MemberNet",
                                          "Player details updated",
                                          TOAST_DISPLAY_TIME, false);
    CNWClient* client = CNWClient::GetClient();
    if (client)
    {
      client->UpdateFromJson(url, machineID, locationID);
      client->Startup(false, ClientFetchUpdatePlayer);
    }
    
  }
  return OK;
}

JSONRPC_STATUS CMNJSONOperations::DumpLogcat(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
//  std::string somestr = g_sysinfo.SaveSystemLog();
//  result = somestr;
  return OK;
}

