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
#include <string>
#include "NWClient.h"
#include "XBDateTime.h"
#include "threads/Thread.h"
#include "threads/CriticalSection.h"
#include "filesystem/CurlFile.h"

#define ENABLE_NWMEDIAMANAGER_DEBUGLOGS 0

typedef void (*AssetUpdateCallBackFn)(const void *ctx, NWAsset &asset, AssetDownloadState downloadState);

class CNWMediaManager : public CThread
{
public:
  CNWMediaManager();
  virtual ~CNWMediaManager();

  void          ClearAssets();
  void          ClearDownloads();
  size_t        GetDownloadCount();
  bool          GetLocalAsset(size_t index, NWAsset &asset);
  bool          GetLocalAsset(std::string asset_id, NWAsset &asset);
  size_t        GetLocalAssetCount();
  void          UpdateNetworkStatus(bool hasNetwork);
  void          QueueAssetsForDownload(std::vector<NWAsset> &assets);
  void          QueueAssetForDownload(NWAsset &asset);
  bool          CheckAssetIsPresentLocal(NWAsset &asset);
  bool          BaseNameInList(const std::string &basename);

  void          RegisterAssetUpdateCallBack(const void *ctx, AssetUpdateCallBackFn fn);

protected:
  virtual void  Process();
  bool          AssetExists(NWAsset &asset);

  XFILE::CCurlFile      m_http;
  bool                  m_hasNetwork;
  std::vector<NWAsset>  m_download;
  CCriticalSection      m_download_lock;

  std::vector<NWAsset>  m_assets;
  CCriticalSection      m_assets_lock;

  AssetUpdateCallBackFn m_AssetUpdateCallBackFn;
  const void           *m_AssetUpdateCallBackCtx;
};
