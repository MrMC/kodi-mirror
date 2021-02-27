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


#include <algorithm>

#include "NWTVAPI.h"

#include "URL.h"
#include "filesystem/CurlFile.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Variant.h"
#include "utils/URIUtils.h"
#include "utils/log.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "ServiceBroker.h"
#include "network/Network.h"


#include <string>
#include <sstream>

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

static std::string TVAPI_URLBASE;

const std::string TVAPI_GetURLBASE()
{
  return TVAPI_URLBASE;
}

void TVAPI_SetURLBASE(std::string urlbase)
{
  // the code expects a trailing slash
  URIUtils::AddSlashAtEnd(urlbase);
  TVAPI_URLBASE = urlbase;
  CLog::Log(LOGDEBUG, "TVAPI_SetURLBASE %s", TVAPI_URLBASE.c_str());
}

// used in v2 and translation into v3
// TVAPI_Machine, TVAPI_PlaylistFile, TVAPI_PlaylistItem, TVAPI_PlaylistItems, TVAPI_Playlist


bool TVAPI_DoActivate(TVAPI_Activate &activate)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "activate");
  curl.SetProtocolOption("Cache-Control", "no-cache");
  curl.SetProtocolOption("Content-Type", "application/x-www-form-urlencoded");

  curl.SetOption("code", activate.code);
  curl.SetOption("application_id", activate.application_id);
  CLog::Log(LOGDEBUG, "TVAPI_DoActivate %s", curl.Get().c_str());

  std::string strResponse;
  if (curlfile.Post(curl.Get(), "", strResponse))
  {
    if (!strResponse.empty())
      CLog::Log(LOGDEBUG, "TVAPI_DoActivate %s", strResponse.c_str());

    CVariant reply;
    CJSONVariantParser::Parse(strResponse, reply);

    std::string message = reply["message"].asString();
    if (message == "Operation Successful")
    {
      activate.apiKey = reply["key"].asString();
      activate.apiSecret = reply["secret"].asString();
      #if ENABLE_TVAPI_DEBUGLOGS
      CLog::Log(LOGDEBUG, "testNationwide5_0 apiKey = %s", activate.apiKey.c_str());
      CLog::Log(LOGDEBUG, "testNationwide5_0 apiSecret = %s", activate.apiSecret.c_str());
      #endif
      return true;
    }
  }

  return false;
}

bool TVAPI_GetStatus(TVAPI_Status &status)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "status");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Cache-Control", "no-cache");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(status.apiKey);
  curl.SetPassword(status.apiSecret);
  std::string strResponse;

  if (curlfile.Get(curl.Get(), strResponse))
  {
    #if ENABLE_TVAPI_DEBUGLOGS
    CLog::Log(LOGDEBUG, "TVAPI_GetStatus %s", strResponse.c_str());
    #endif

    CVariant reply;
    CJSONVariantParser::Parse(strResponse, reply);

    status.key             = reply["key"].asString();
    status.unique_id       = reply["unique_id"].asString();
    status.machine_id      = reply["machine_id"].asString();
    status.status          = reply["status"].asString();
    status.status_text     = reply["status_text"].asString();
    status.activation_date = reply["activation_date"].asString();
    return true;
  }

  return false;
}

bool TVAPI_GetMachine(TVAPI_Machine &machine)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "machine");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Cache-Control", "no-cache");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(machine.apiKey);
  curl.SetPassword(machine.apiSecret);
  std::string strResponse;

  if (curlfile.Get(curl.Get(), strResponse))
  {
    #if ENABLE_TVAPI_DEBUGLOGS
    CLog::Log(LOGDEBUG, "TVAPI_GetMachine %s", strResponse.c_str());
    #endif
    CVariant reply;
    CJSONVariantParser::Parse(strResponse, reply);

    machine.id                    = reply["id"].asString();
    machine.member                = reply["member"].asString();
    machine.machine_name          = reply["machine_name"].asString();
    machine.description           = reply["description"].asString();
    machine.playlist_id           = reply["playlist_id"].asString();
    machine.status                = reply["status"].asString();
    machine.vendor                = reply["vendor"].asString();
    machine.hardware              = reply["hardware"].asString();
    machine.timezone              = reply["timezone"].asString();
    machine.serial_number         = reply["serial_number"].asString();
    machine.warranty_number       = reply["warranty_number"].asString();
    machine.video_format          = reply["video_format"].asString();
    machine.allow_new_content     = reply["allow_new_content"].asString();
    machine.allow_software_update = reply["allow_software_update"].asString();
    machine.update_time           = reply["update_time"].asString();
    machine.update_interval       = reply["update_interval"].asString();

    CVariant location             = reply["location"];
    machine.location.id           = location["id"].asString();
    machine.location.name         = location["name"].asString();
    machine.location.address      = location["address"].asString();
    machine.location.address2     = location["address2"].asString();
    machine.location.city         = location["city"].asString();
    machine.location.state        = location["state"].asString();
    machine.location.phone        = location["phone"].asString();
    machine.location.fax          = location["fax"].asString();

    CVariant network              = reply["network"];
    machine.network.macaddress    = network["macaddress"].asString();
    machine.network.macaddress_wireless = network["macaddress_wireless"].asString();
    machine.network.dhcp          = network["dhcp"].asString();
    machine.network.ipaddress     = network["ipaddress"].asString();
    machine.network.subnet        = network["subnet"].asString();
    machine.network.router        = network["router"].asString();
    machine.network.dns_1         = network["dns_1"].asString();
    machine.network.dns_2         = network["dns_2"].asString();

    CVariant settings             = reply["settings"];
    machine.settings.network      = settings["network"].asString();
    machine.settings.pairing      = settings["pairing"].asString();
    machine.settings.network      = settings["network"].asString();
    machine.settings.about        = settings["about"].asString();
    machine.settings.hdmibrightness = settings["hdmibrightness"].asString();
    machine.settings.tvresolution = settings["tvresolution"].asString();
    machine.settings.updatesoftware = settings["updatesoftware"].asString();
    machine.settings.language     = settings["language"].asString();
    machine.settings.legal        = settings["legal"].asString();

    CVariant menu                 = reply["menu"];
    machine.menu.membernettv      = menu["membernettv"].asString();
    machine.menu.vendorcommercials = menu["vendorcommercials"].asString();
    machine.menu.hdcontent        = menu["hdcontent"].asString();
    machine.menu.membercommercials = menu["membercommercials"].asString();
    machine.menu.movietrailers    = menu["movietrailers"].asString();
    machine.menu.promotionalcampaigns = menu["promotionalcampaigns"].asString();
    machine.menu.nationwidebroadcasts = menu["nationwidebroadcasts"].asString();
    machine.menu.imaginationwidehd = menu["imaginationwidehd"].asString();
    machine.menu.primemediacommercialfactory = menu["primemediacommercialfactory"].asString();

    CVariant membernet_software   = reply["membernet_software"];
    machine.membernet_software.id = membernet_software["id"].asString();
    machine.membernet_software.version = membernet_software["version"].asString();
    machine.membernet_software.cfbundleversion = membernet_software["cfbundleversion"].asString();
    machine.membernet_software.url = membernet_software["url"].asString();

    CVariant apple_software       = reply["apple_software"];
    machine.apple_software.id     = apple_software["id"].asString();
    machine.apple_software.version = apple_software["version"].asString();
    machine.apple_software.url    = apple_software["url"].asString();

    return true;
  }

  return false;
}

bool TVAPI_UpdateMachineInfo(TVAPI_MachineUpdate &machineUpdate)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "machine");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Cache-Control", "no-cache");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(machineUpdate.apiKey);
  curl.SetPassword(machineUpdate.apiSecret);

  CVariant params;
  params["status"] = machineUpdate.status;
  params["macaddress"] = machineUpdate.macaddress;
  params["mac_address_wireless"] = machineUpdate.macaddress_wireless;

  std::string jsonBody;
  CJSONVariantWriter::Write(params,jsonBody, false);
  std::string strResponse;
  if (curlfile.Put(curl.Get(), jsonBody, strResponse))
  {
    #if ENABLE_TVAPI_DEBUGLOGS
    if (!strResponse.empty())
      CLog::Log(LOGDEBUG, "TVAPI_UpdateMachineInfo %s", strResponse.c_str());
    #endif
    return true;
  }
  return false;
}

bool TVAPI_GetPlaylists(TVAPI_Playlists &playlists)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  std::string sub_url;
  sub_url = TVAPI_URLBASE + "playlist";
  sub_url += "?_page=" + std_to_string(1) + "&_perPage=50";

  CURL curl(sub_url);
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Cache-Control", "no-cache");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(playlists.apiKey);
  curl.SetPassword(playlists.apiSecret);
  std::string strResponse;

  if (curlfile.Get(curl.Get(), strResponse))
  {
    #if ENABLE_TVAPI_DEBUGLOGS
    CLog::Log(LOGDEBUG, "TVAPI_GetPlaylists %s", strResponse.c_str());
    #endif

    CVariant reply;
    CJSONVariantParser::Parse(strResponse, reply);

    int page = 1;
    int total = reply["total"].asInteger();
    int sub_total = 0;
    playlists.playlists.clear();
    while(true)
    {
      #if ENABLE_TVAPI_DEBUGLOGS
      int curPage = reply["page"].asInteger();
      int perPage = reply["perPage"].asInteger();
      #endif

      CVariant results(CVariant::VariantTypeArray);
      results = reply["results"];
      for (size_t i = 0; i < results.size(); i++, sub_total++)
      {
        CVariant result = results[i];

        TVAPI_PlaylistInfo playListInfo;
        playListInfo.id = result["id"].asString();
        playListInfo.name = result["name"].asString();
        playListInfo.type = result["type"].asString();
        playListInfo.updated_date = result["updated_date"].asString();
        playListInfo.layout = result["layout"].asString();
        playListInfo.member_id = result["member_id"].asString();
        playListInfo.member_name = result["member_name"].asString();
        playListInfo.nmg_managed = result["nmg_managed"].asString();
        playlists.playlists.push_back(playListInfo);

        #if ENABLE_TVAPI_DEBUGLOGS
        CLog::Log(LOGDEBUG, "TVAPI_GetPlaylists %d, %s", sub_total, playListInfo.name.c_str());
        #endif
      }

      #if ENABLE_TVAPI_DEBUGLOGS
      CLog::Log(LOGDEBUG, "TVAPI_GetPlaylists page = %d, perPage = %d, sub_total = %d, total = %d", curPage, perPage, sub_total, total);
      #endif

      if (sub_total < total)
      {
        sub_url = TVAPI_URLBASE + "playlist";
        sub_url += "?_page=" + std_to_string(++page) + "&_perPage=25";
        curl = CURL(sub_url);
        if (curlfile.Get(curl.Get(), strResponse))
        {
          CJSONVariantParser::Parse(strResponse, reply);
          continue;
        }
      }

      // if we get here, we are done
      break;
    }

    return true;
  }

  return false;
}

bool TVAPI_GetPlaylist(TVAPI_Playlist &playlist, std::string playlist_id)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "playlist/" + playlist_id);
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Cache-Control", "no-cache");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(playlist.apiKey);
  curl.SetPassword(playlist.apiSecret);
  std::string strResponse;

  if (curlfile.Get(curl.Get(), strResponse))
  {
    #if ENABLE_TVAPI_DEBUGLOGS
    CLog::Log(LOGDEBUG, "TVAPI_GetPlaylist %s", strResponse.c_str());
    #endif

    CVariant reply;
    CJSONVariantParser::Parse(strResponse, reply);

    playlist.id = reply["id"].asString();
    playlist.name = reply["name"].asString();
    playlist.type = reply["type"].asString();
    playlist.layout = reply["layout"].asString();
    playlist.member_id = reply["member_id"].asString();
    playlist.nmg_managed = reply["nmg_managed"].asString();
    playlist.updated_date = reply["updated_date"].asString();

    if (playlist.type == "smart")
    {
      CVariant categories(CVariant::VariantTypeArray);
      categories = reply["categories"];
      for (size_t i = 0; i < categories.size(); ++i)
      {
        TVAPI_CategoryId category;
        category.id = categories[i]["id"].asString();
        category.name = categories[i]["name"].asString();
        playlist.categories.push_back(category);
      }
    }
    else if (playlist.type == "custom")
    {
      CVariant files(CVariant::VariantTypeArray);
      files = reply["files"];
      for (size_t i = 0; i < files.size(); ++i)
      {
        TVAPI_FileId file;
        file.id = files[i]["id"].asString();
        playlist.files.push_back(file);
      }
    }

    return true;
  }

  return false;
}

bool TVAPI_GetPlaylistItems(TVAPI_PlaylistItems &playlistItems, std::string playlist_id)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "playlist/" + playlist_id + "/files");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Cache-Control", "no-cache");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(playlistItems.apiKey);
  curl.SetPassword(playlistItems.apiSecret);
  std::string strResponse;

  if (curlfile.Get(curl.Get(), strResponse))
  {
    #if ENABLE_TVAPI_DEBUGLOGS
    CLog::Log(LOGDEBUG, "TVAPI_GetPlaylistItems %s", strResponse.c_str());
    #endif

    CVariant reply;
    CJSONVariantParser::Parse(strResponse, reply);

    CVariant playlist = reply["playlist"];
    playlistItems.id = playlist["id"].asString();
    playlistItems.name = playlist["name"].asString();
    playlistItems.type = playlist["type"].asString();

    CVariant results(CVariant::VariantTypeArray);
    results = reply["results"];
    for (size_t i = 0; i < results.size(); ++i)
    {
      CVariant result = results[i];

      TVAPI_PlaylistItem item;
      item.id = result["id"].asString();
      item.name = result["name"].asString();
      item.tv_category_id = result["tv_category_id"].asString();
      item.description = result["description"].asString();
      item.created_date = result["created_date"].asString();
      item.updated_date = result["updated_date"].asString();
      item.completion_date = result["completion_date"].asString();
      item.theatricalrelease = result["theatricalrelease"].asString();
      item.dvdrelease = result["dvdrelease"].asString();
      item.download = result["download"].asString();
      CVariant availability = result["availability"];
      item.availability_to = availability["to"].asString();
      item.availability_from = availability["from"].asString();

      CVariant files = result["files"];
      if (files.isObject())
      {
        for (CVariant::const_iterator_map it = files.begin_map(); it != files.end_map(); ++it)
        {
          CVariant fileobj = it->second;
          if (fileobj.isObject())
          {
            if (it->first == "720" || it->first == "1080" || it->first == "4K")
            {
              TVAPI_PlaylistFile file;
              file.type = it->first;
              file.path = fileobj["path"].asString();
              file.size = fileobj["size"].asString();
              file.width = fileobj["width"].asString();
              file.height = fileobj["height"].asString();
              file.etag = fileobj["etag"].asString();
              file.mime_type = fileobj["mime_type"].asString();
              file.created_date = fileobj["created_date"].asString();
              file.updated_date = fileobj["updated_date"].asString();
              item.files.push_back(file);
            }
          }
        }
        // sort from low rez to high rez
        std::sort(item.files.begin(), item.files.end(),
          [] (TVAPI_PlaylistFile const& a, TVAPI_PlaylistFile const& b)
          {
            return std_stoi(a.type) < std_stoi(b.type);
          });

        // now find the 'thumb'
        for (CVariant::const_iterator_map it = files.begin_map(); it != files.end_map(); ++it)
        {
          CVariant fileobj = it->second;
          if (fileobj.isObject() && it->first == "thumb")
          {
            item.thumb.type = it->first;
            item.thumb.path = fileobj["path"].asString();
            item.thumb.size = fileobj["size"].asString();
            item.thumb.width = fileobj["width"].asString();
            item.thumb.height = fileobj["height"].asString();
            item.thumb.etag = fileobj["etag"].asString();
            item.thumb.mime_type = fileobj["mime_type"].asString();
            item.thumb.created_date = fileobj["created_date"].asString();
            item.thumb.updated_date = fileobj["updated_date"].asString();
          }
        }

        // and find the 'poster'
        for (CVariant::const_iterator_map it = files.begin_map(); it != files.end_map(); ++it)
        {
          CVariant fileobj = it->second;
          if (fileobj.isObject() && it->first == "thumb")
          {
            item.poster.type = it->first;
            item.poster.path = fileobj["path"].asString();
            item.poster.size = fileobj["size"].asString();
            item.poster.width = fileobj["width"].asString();
            item.poster.height = fileobj["height"].asString();
            item.poster.etag = fileobj["etag"].asString();
            item.poster.mime_type = fileobj["mime_type"].asString();
            item.poster.created_date = fileobj["created_date"].asString();
            item.poster.updated_date = fileobj["updated_date"].asString();
          }
        }

      }
      playlistItems.items.push_back(item);
    }

    return true;
  }

  return false;
}

bool TVAPI_ReportHealth(TVAPI_HealthReport &health)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "health");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Content-Type", "application/x-www-form-urlencoded");
  curl.SetUserName(health.apiKey);
  curl.SetPassword(health.apiSecret);

  // parameters
  std::string params;
  params = "date=" + health.date;
  params += "&uptime=" + health.uptime;
  params += "&disk_used=" + health.disk_used;
  params += "&disk_free=" + health.disk_free;
  params += "&smart_status=" + health.smart_status;

  std::string strResponse;
  if (curlfile.Post(curl.Get(), params, strResponse))
  {
    if (!strResponse.empty())
      CLog::Log(LOGDEBUG, "TVAPI_ReportHealth %s", strResponse.c_str());
    return true;
  }
  return false;
}

bool TVAPI_ReportFilesPlayed(TVAPI_Files &files, std::string serial_number)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "file-played");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetOption("serial_number", serial_number);
  curl.SetUserName(files.apiKey);
  curl.SetPassword(files.apiSecret);

  CVariant vfiles(CVariant::VariantTypeArray);
  for (size_t i = 0; i < files.files.size(); ++i)
  {
    CVariant file;
    file["id"] = files.files[i].id;
    file["date"] = files.files[i].date;
    file["format"] = files.files[i].format;
    vfiles.push_back(file);
  }
  std::string jsonBody;
  CJSONVariantWriter::Write(vfiles,jsonBody, false);
  std::string strResponse;
  if (curlfile.Post(curl.Get(), jsonBody, strResponse))
  {
    if (!strResponse.empty())
      CLog::Log(LOGDEBUG, "TVAPI_ReportFilesPlayed %s", strResponse.c_str());
    return true;
  }

  return false;
}

bool TVAPI_ReportFilesDeleted(TVAPI_Files &files)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "file-downloaded");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(files.apiKey);
  curl.SetPassword(files.apiSecret);

  CVariant vfiles(CVariant::VariantTypeArray);
  for (size_t i = 0; i < files.files.size(); ++i)
  {
    CVariant file;
    file["id"] = files.files[i].id;
    file["date"] = files.files[i].date;
    vfiles.push_back(file);
  }
  std::string jsonBody;
  CJSONVariantWriter::Write(vfiles,jsonBody, false);
  std::string strResponse;
  if (curlfile.Delete(curl.Get(), jsonBody, strResponse))
  {
    if (!strResponse.empty())
      CLog::Log(LOGDEBUG, "TVAPI_ReportFilesDeleted %s", strResponse.c_str());
    return true;
  }

  return false;
}

bool TVAPI_ReportFilesDownloaded(TVAPI_Files &files)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "file-downloaded");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(files.apiKey);
  curl.SetPassword(files.apiSecret);

  CVariant vfiles(CVariant::VariantTypeArray);
  for (size_t i = 0; i < files.files.size(); ++i)
  {
    CVariant file;
    file["id"] = files.files[i].id;
    file["date"] = files.files[i].date;
    vfiles.push_back(file);
  }
  std::string jsonBody;
  CJSONVariantWriter::Write(vfiles,jsonBody, false);
  std::string strResponse;
  if (curlfile.Post(curl.Get(), jsonBody, strResponse))
  {
    if (!strResponse.empty())
      CLog::Log(LOGDEBUG, "TVAPI_ReportFilesDownloaded %s", strResponse.c_str());
    return true;
  }

  return false;
}

bool TVAPI_GetActionQueue(TVAPI_Actions &actions)
{
  bool rtn = false;
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "machine-actions");
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Cache-Control", "no-cache");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(actions.apiKey);
  curl.SetPassword(actions.apiSecret);

  std::string strResponse;
  if (curlfile.Get(curl.Get(), strResponse))
  {
    #if ENABLE_TVAPI_DEBUGLOGS
    if (!strResponse.empty())
      CLog::Log(LOGDEBUG, "TVAPI_GetActionQueue %s", strResponse.c_str());
    #endif

    CVariant results(CVariant::VariantTypeArray);
    CJSONVariantParser::Parse(strResponse, results);
    for (size_t i = 0; i < results.size(); ++i)
    {
      CVariant result = results[i];

      TVAPI_Action action;
      action.id = result["id"].asString();
      action.action = result["action"].asString();
      action.name = result["name"].asString();
      action.status = result["status"].asString();
      action.created_date = result["created_date"].asString();
      actions.actions.push_back(action);
      rtn = true;

      #if ENABLE_TVAPI_DEBUGLOGS
      CLog::Log(LOGDEBUG, "TVAPI_GetActionQueue %s, %s", action.action.c_str(), action.name.c_str());
      #endif
    }
  }

  return rtn;
}

bool TVAPI_UpdateActionStatus(TVAPI_ActionStatus &actionStatus)
{
  XFILE::CCurlFile curlfile;
  curlfile.SetTimeout(10);

  CURL curl(TVAPI_URLBASE + "machine-actions/" + actionStatus.id);
  curl.SetProtocolOption("seekable", "0");
  curl.SetProtocolOption("auth", "basic");
  curl.SetProtocolOption("Content-Type", "application/json");
  curl.SetUserName(actionStatus.apiKey);
  curl.SetPassword(actionStatus.apiSecret);

  CVariant params;
  params["status"] = actionStatus.status;
  if (!actionStatus.message.empty())
    params["message"] = actionStatus.message;

  std::string jsonBody;
  CJSONVariantWriter::Write(params,jsonBody, false);
  std::string strResponse;
  if (curlfile.Put(curl.Get(), jsonBody, strResponse))
  {
    #if ENABLE_TVAPI_DEBUGLOGS
    if (!strResponse.empty())
      CLog::Log(LOGDEBUG, "TVAPI_UpdateActionStatus %s", strResponse.c_str());
    #endif
    return true;
  }
  return false;
}

int TVAPI_GetApiVersion()
{
  return CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::MN_API_VERSION);
}

void TVAPI_SetApiVersion(int version)
{
  CServiceBroker::GetSettingsComponent()->GetSettings()->SetInt(CSettings::MN_API_VERSION, version);
  CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
}
