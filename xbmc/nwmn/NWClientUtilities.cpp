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

#include "NWClientUtilities.h"

#include "Application.h"
#include "LangInfo.h"
#include "ServiceBroker.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#include "filesystem/SpecialProtocol.h"
#include "network/Network.h"
#include "storage/MediaManager.h"
#include "utils/StringUtils.h"
#include "utils/XBMCTinyXML.h"
#include "utils/XMLUtils.h"
//#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/SystemInfo.h"
#include "utils/TimeUtils.h"

#include "platform/darwin/DarwinUtils.h"

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

// ---------------------------------------------
// ---------------------------------------------
bool HasLocalPlayer(std::string home)
{
  return XFILE::CFile::Exists(home + kNWClient_PlayerFileName);
}

bool RemoveLocalPlayer(std::string home)
{
  return XFILE::CFile::Delete(home + kNWClient_PlayerFileName);
}

bool LoadLocalPlayer(std::string home, NWPlayerInfo &playerInfo)
{
  CXBMCTinyXML xmlDoc;
  if (!xmlDoc.LoadFile(home + kNWClient_PlayerFileName))
    return false;

  TiXmlElement *rootElement = xmlDoc.RootElement();
  if (!rootElement)
    return false;

  XMLUtils::GetString(rootElement, "id", playerInfo.id);
  XMLUtils::GetString(rootElement, "name", playerInfo.name);
  XMLUtils::GetString(rootElement, "member", playerInfo.member);
  XMLUtils::GetString(rootElement, "vendor", playerInfo.vendor);
  XMLUtils::GetString(rootElement, "description", playerInfo.description);
  XMLUtils::GetString(rootElement, "serial_number", playerInfo.serial_number);
  XMLUtils::GetString(rootElement, "warranty_number", playerInfo.warranty_number);
  XMLUtils::GetString(rootElement, "macaddress", playerInfo.macaddress);
  XMLUtils::GetString(rootElement, "macaddress_wireless", playerInfo.macaddress_wireless);
  XMLUtils::GetString(rootElement, "hardware_version", playerInfo.hardware_version);
  XMLUtils::GetString(rootElement, "software_version", playerInfo.software_version);

  XMLUtils::GetString(rootElement, "playlist_id", playerInfo.playlist_id);
  XMLUtils::GetString(rootElement, "video_format", playerInfo.video_format);
  XMLUtils::GetString(rootElement, "update_time", playerInfo.update_time);
  XMLUtils::GetString(rootElement, "update_interval", playerInfo.update_interval);
  XMLUtils::GetString(rootElement, "allow_new_content", playerInfo.allow_new_content);
  XMLUtils::GetString(rootElement, "allow_software_update", playerInfo.allow_software_update);
  XMLUtils::GetString(rootElement, "allow_async_player", playerInfo.allow_async_player);
  XMLUtils::GetString(rootElement, "status", playerInfo.status);
  XMLUtils::GetString(rootElement, "apiKey", playerInfo.apiKey);
  XMLUtils::GetString(rootElement, "apiSecret", playerInfo.apiSecret);
  XMLUtils::GetString(rootElement, "tvapiURLBase", playerInfo.tvapiURLBase);

  return true;
}

bool SaveLocalPlayer(std::string home, const NWPlayerInfo &playerInfo)
{
  CXBMCTinyXML xmlDoc;

  TiXmlElement rootElement("player");
  TiXmlNode *rootNode = xmlDoc.InsertEndChild(rootElement);
  if (rootNode)
  {
    XMLUtils::SetString(rootNode, "id", playerInfo.id);
    XMLUtils::SetString(rootNode, "name", playerInfo.name);
    XMLUtils::SetString(rootNode, "member", playerInfo.member);
    XMLUtils::SetString(rootNode, "vendor", playerInfo.vendor);
    XMLUtils::SetString(rootNode, "description", playerInfo.description);
    XMLUtils::SetString(rootNode, "serial_number", playerInfo.serial_number);
    XMLUtils::SetString(rootNode, "warranty_number", playerInfo.warranty_number);
    XMLUtils::SetString(rootNode, "macaddress", playerInfo.macaddress);
    XMLUtils::SetString(rootNode, "macaddress_wireless", playerInfo.macaddress_wireless);
    XMLUtils::SetString(rootNode, "hardware_version", playerInfo.hardware_version);
    XMLUtils::SetString(rootNode, "software_version", playerInfo.software_version);

    XMLUtils::SetString(rootNode, "playlist_id", playerInfo.playlist_id);
    XMLUtils::SetString(rootNode, "video_format", playerInfo.video_format);
    XMLUtils::SetString(rootNode, "update_time", playerInfo.update_time);
    XMLUtils::SetString(rootNode, "update_interval", playerInfo.update_interval);
    XMLUtils::SetString(rootNode, "allow_new_content", playerInfo.allow_new_content);
    XMLUtils::SetString(rootNode, "allow_software_update", playerInfo.allow_software_update);
    XMLUtils::SetString(rootNode, "allow_async_player", playerInfo.allow_async_player);
    XMLUtils::SetString(rootNode, "status", playerInfo.status);
    XMLUtils::SetString(rootNode, "apiKey", playerInfo.apiKey);
    XMLUtils::SetString(rootNode, "apiSecret", playerInfo.apiSecret);
    XMLUtils::SetString(rootNode, "tvapiURLBase", playerInfo.tvapiURLBase);

    return xmlDoc.SaveFile(home + kNWClient_PlayerFileName);
  }
  
  return false;
}

bool HasLocalPlaylist(std::string home)
{
  return XFILE::CFile::Exists(home + kNWClient_PlaylistFileName);
}

bool LoadLocalPlaylist(std::string home, NWPlaylist &playList)
{
  CXBMCTinyXML xmlDoc;
  if (!xmlDoc.LoadFile(home + kNWClient_PlaylistFileName))
    return false;

  TiXmlElement *rootElement = xmlDoc.RootElement();
  if (!rootElement)
    return false;

  XMLUtils::GetString(rootElement, "id", playList.id);
  XMLUtils::GetString(rootElement, "name", playList.name);
  XMLUtils::GetString(rootElement, "type", playList.type);
  XMLUtils::GetString(rootElement, "video_format", playList.video_format);
  XMLUtils::GetString(rootElement, "layout", playList.layout);
  XMLUtils::GetString(rootElement, "updated_date", playList.updated_date);

  std::string play_order;
  XMLUtils::GetString(rootElement, "play_order", play_order);
  std::vector<std::string> orders = StringUtils::Split(play_order, ',');
  playList.play_order.clear();
  for (auto order: orders)
  {
    if (!order.empty())
      playList.play_order.push_back(order);
  }

  playList.groups.clear();

  // no groups is a failure
  const TiXmlElement *groupElement = rootElement->FirstChildElement("group");
  if (!groupElement)
    return false;

  while (groupElement != NULL)
  {
    NWGroup group;
    XMLUtils::GetString(groupElement, "id", group.id);
    XMLUtils::GetString(groupElement, "name", group.name);
    XMLUtils::GetInt(   groupElement, "next_asset_index", group.next_asset_index);

    // no assets is not a failure
    const TiXmlElement *assetElement = groupElement->FirstChildElement("asset");
    while (assetElement != nullptr)
    {
      NWAsset asset;
      XMLUtils::GetString(assetElement, "id", asset.id);
      XMLUtils::GetString(assetElement, "group_id", asset.group_id);
      XMLUtils::GetString(assetElement, "name", asset.name);
      XMLUtils::GetString(assetElement, "type", asset.type);

      XMLUtils::GetString(assetElement, "video_url", asset.video_url);
      XMLUtils::GetString(assetElement, "video_md5", asset.video_md5);
      XMLUtils::GetInt(   assetElement, "video_size", asset.video_size);
      XMLUtils::GetString(assetElement, "video_basename", asset.video_basename);
      XMLUtils::GetString(assetElement, "video_localpath", asset.video_localpath);
      XMLUtils::GetString(assetElement, "video_format", asset.type);

      XMLUtils::GetString(assetElement, "thumb_url", asset.thumb_url);
      XMLUtils::GetString(assetElement, "thumb_md5", asset.thumb_md5);
      XMLUtils::GetInt(   assetElement, "thumb_size", asset.thumb_size);
      XMLUtils::GetString(assetElement, "thumb_basename", asset.thumb_basename);
      XMLUtils::GetString(assetElement, "thumb_localpath", asset.thumb_localpath);

      std::string availability;
      XMLUtils::GetString(assetElement, "available_to", availability);
      asset.available_to.SetFromDBDateTime(availability);
      XMLUtils::GetString(assetElement, "available_from", availability);
      asset.available_from.SetFromDBDateTime(availability);

      asset.valid = false;
      group.assets.push_back(asset);
      assetElement = assetElement->NextSiblingElement("asset");
    }
    playList.groups.push_back(group);

    groupElement = groupElement->NextSiblingElement("group");
  }

  return true;
}

bool SaveLocalPlaylist(std::string home, const NWPlaylist &playList)
{
  CXBMCTinyXML xmlDoc;

  TiXmlElement rootElement("playlist");
  TiXmlNode *rootNode = xmlDoc.InsertEndChild(rootElement);
  if (rootNode)
  {
    XMLUtils::SetString(rootNode, "id", playList.id);
    XMLUtils::SetString(rootNode, "name", playList.name);
    XMLUtils::SetString(rootNode, "type", playList.type);
    XMLUtils::SetString(rootNode, "video_format", playList.video_format);
    XMLUtils::SetString(rootNode, "layout", playList.layout);
    XMLUtils::SetString(rootNode, "updated_date", playList.updated_date);
    std::string play_order;
    for (auto order: playList.play_order)
      play_order += order + ",";
    // remove the trailing ','
    play_order.pop_back();
    XMLUtils::SetString(rootNode, "play_order", play_order);

    for (auto group: playList.groups)
    {
      TiXmlElement groupElement("group");
      TiXmlNode *groupNode = rootNode->InsertEndChild(groupElement);
      if (groupNode)
      {
        XMLUtils::SetString(groupNode, "id", group.id);
        XMLUtils::SetString(groupNode, "name", group.name);
        XMLUtils::SetInt(   groupNode, "next_asset_index", group.next_asset_index);
        for (auto asset: group.assets)
        {
          TiXmlElement assetElement("asset");
          TiXmlNode *assetNode = groupNode->InsertEndChild(assetElement);
          if (assetNode)
          {
            XMLUtils::SetString(assetNode, "id", asset.id);
            XMLUtils::SetString(assetNode, "group_id", asset.group_id);
            XMLUtils::SetString(assetNode, "name", asset.name);
            XMLUtils::SetString(assetNode, "type", asset.type);

            XMLUtils::SetString(assetNode, "video_url", asset.video_url);
            XMLUtils::SetString(assetNode, "video_md5", asset.video_md5);
            XMLUtils::SetInt(   assetNode, "video_size", asset.video_size);
            XMLUtils::SetString(assetNode, "video_basename", asset.video_basename);
            XMLUtils::SetString(assetNode, "video_localpath", asset.video_localpath);
            XMLUtils::SetString(assetNode, "video_format", asset.type);

            XMLUtils::SetString(assetNode, "thumb_url", asset.thumb_url);
            XMLUtils::SetString(assetNode, "thumb_md5", asset.thumb_md5);
            XMLUtils::SetInt(   assetNode, "thumb_size", asset.thumb_size);
            XMLUtils::SetString(assetNode, "thumb_basename", asset.thumb_basename);
            XMLUtils::SetString(assetNode, "thumb_localpath", asset.thumb_localpath);

            XMLUtils::SetString(assetNode, "available_to", asset.available_to.GetAsDBDateTime());
            XMLUtils::SetString(assetNode, "available_from", asset.available_from.GetAsDBDateTime());
          }
        }
      }
    }

    return xmlDoc.SaveFile(home + kNWClient_PlaylistFileName);
  }

  return false;
}

std::string GetDiskUsed(std::string path)
{
  std::string used;
  std::string home = CSpecialProtocol::TranslatePath(path);

  std::vector<std::string> diskUsage = CServiceBroker::GetMediaManager().GetDiskUsage();
  for (size_t d = 1; d < diskUsage.size(); d++)
  {
    StringUtils::RemoveCRLF(diskUsage[d]);
    StringUtils::RemoveDuplicatedSpacesAndTabs(diskUsage[d]);

    std::vector<std::string> items = StringUtils::Split(diskUsage[d], " ");
    #if defined(TARGET_ANDROID)
      used = items[2];
      StringUtils::Replace(used, "G", "GB");
      break;
    #else
      std::string mountPoint = items[items.size() - 1];
      if (mountPoint == "/")
        used = items[2];
      if (mountPoint.find(path) != std::string::npos)
        used = items[2];
      StringUtils::Replace(used, "Gi", "GB");
    #endif
  }

  return used;
}

std::string GetDiskFree(std::string path)
{
  std::string free;
  std::string home = CSpecialProtocol::TranslatePath(path);

  std::vector<std::string> diskUsage = CServiceBroker::GetMediaManager().GetDiskUsage();
  for (size_t d = 1; d < diskUsage.size(); d++)
  {
    StringUtils::RemoveCRLF(diskUsage[d]);
    StringUtils::RemoveDuplicatedSpacesAndTabs(diskUsage[d]);

    std::vector<std::string> items = StringUtils::Split(diskUsage[d], " ");
    #if defined(TARGET_ANDROID)
      free = items[3];
      StringUtils::Replace(free, "G", "GB");
      break;
    #else
      std::string mountPoint = items[items.size() - 1];
      if (mountPoint == "/" || mountPoint.find(path) != std::string::npos)
      {
        free = items[3];
        StringUtils::Replace(free, "Gi", "GB");
        break;
      }
    #endif
  }

  return free;
}

std::string GetDiskTotal(std::string path)
{
  std::string total;
  std::string home = CSpecialProtocol::TranslatePath(path);

  std::vector<std::string> diskUsage = CServiceBroker::GetMediaManager().GetDiskUsage();
  for (size_t d = 1; d < diskUsage.size(); d++)
  {
    StringUtils::RemoveCRLF(diskUsage[d]);
    StringUtils::RemoveDuplicatedSpacesAndTabs(diskUsage[d]);

    std::vector<std::string> items = StringUtils::Split(diskUsage[d], " ");
    #if defined(TARGET_ANDROID)
      total = items[1];
      StringUtils::Replace(total, "G", "GB");
      break;
    #else
      std::string mountPoint = items[items.size() - 1];
      if (mountPoint == "/")
        total = items[1];
      if (mountPoint.find(path) != std::string::npos)
        total = items[1];
      StringUtils::Replace(total, "Gi", "GB");
    #endif
  }

  return total;
}

std::string GetSystemUpTime()
{
  // uptime=0 days 5 hours 22 minutes
  int iMinutes = 0; int iHours = 0; int iDays = 0;
  iMinutes = (int)(CTimeUtils::GetFrameTime() / 60000);
  if (iMinutes >= 60) // Hour's
  {
    iHours = iMinutes / 60;
    iMinutes = iMinutes - (iHours *60);
  }
  if (iHours >= 24) // Days
  {
    iDays = iHours / 24;
    iHours = iHours - (iDays * 24);
  }

  std::string uptime = StringUtils::Format("%d day%s %d hours %d minutes",
                                           iDays,
                                           iDays > 1 ? "s" :"",
                                           iHours,
                                           iMinutes);
  return uptime;
}

const std::string GetWiredMACAddress()
{
  std::vector<CNetworkInterface*>& ifaces = CServiceBroker::GetNetwork().GetInterfaceList();
  for (std::vector<CNetworkInterface*>::const_iterator it = ifaces.begin(); it != ifaces.end(); ++it)
  {
//      if (!(*it)->IsWireless())
        return CServiceBroker::GetNetwork().GetFirstConnectedInterface()->GetMacAddress();
  }
  return "00:00:00:00:00:00";
}

const std::string GetWirelessMACAddress()
{
  std::vector<CNetworkInterface*>& ifaces = CServiceBroker::GetNetwork().GetInterfaceList();
  for (std::vector<CNetworkInterface*>::const_iterator it = ifaces.begin(); it != ifaces.end(); ++it)
  {
//    if ((*it)->IsWireless())
      return CServiceBroker::GetNetwork().GetFirstConnectedInterface()->GetMacAddress();
  }
  return "00:00:00:00:00:00";
}

const std::string GetNUCMACAddress()
{

//  CDarwinUtils::GetMacForAllInterfaces();
  std::vector<CNetworkInterface*>& ifaces = CServiceBroker::GetNetwork().GetInterfaceList();
  for (std::vector<CNetworkInterface*>::const_iterator it = ifaces.begin(); it != ifaces.end(); ++it)
  {
    std::string addy = (*it)->GetMacAddress();
    if (StringUtils::StartsWithNoCase(addy,"1c:69:7a"))
    {
        return addy;
    }
  }

  return CServiceBroker::GetNetwork().GetFirstConnectedInterface()->GetMacAddress();
}

bool HasInternet()
{
  XFILE::CCurlFile http;
  return http.IsInternet();
}

bool isStreamed(NWAsset &asset)
{
  asset.valid = true;
  return true;
}
