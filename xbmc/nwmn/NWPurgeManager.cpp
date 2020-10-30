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

#include <algorithm>

#include "system.h"

#if defined(TARGET_ANDROID) || defined(TARGET_LINUX)
#include <sys/statfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include "NWPurgeManager.h"

#include "Util.h"
#include "FileItem.h"
#include "filesystem/File.h"
#include "filesystem/Directory.h"
#include "filesystem/SpecialProtocol.h"

#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/SystemInfo.h"
#include "utils/URIUtils.h"

//#define DEBUG_NO_DELETEFILES
//#define DEBUG_PURGEWAIT_TIMEOUT

#define PURGEWAIT_TIMEOUTMS (1000 * 60 * 10)

typedef struct FILEINFO {
  std::string name;
  std::string path;
  CDateTime atime;
  CDateTime mtime;
  CDateTime ctime;
} FILEINFO;

CNWPurgeManager::CNWPurgeManager()
 : CThread("CNWPurgeManager")
 , m_canPurge(false)
{
}

CNWPurgeManager::~CNWPurgeManager()
{
  m_bStop = true;
  m_processSleep.Set();
  StopThread();
}

void CNWPurgeManager::AddPurgePath(const std::string &media, const std::string &thumb)
{
  CSingleLock lock(m_paths_lock);

  // do not add duplicate media purge paths
  auto mediapath = std::find(m_media_paths.begin(), m_media_paths.end(), media);
  if (mediapath == m_media_paths.end())
    m_media_paths.push_back(media);

  // do not add duplicate thumb purge paths
  auto thumbpath = std::find(m_thumb_paths.begin(), m_thumb_paths.end(), thumb);
  if (thumbpath == m_thumb_paths.end())
    m_thumb_paths.push_back(thumb);
}

void CNWPurgeManager::Process()
{
  SetPriority(THREAD_PRIORITY_BELOW_NORMAL);
  #if ENABLE_NWMPURGEMANAGER_DEBUGLOGS
  CLog::Log(LOGDEBUG, "**NW** - CNWPurgeManager::Process Started");
  #endif

  while (!m_bStop)
  {
    // want wait delay to happen at top so
    // mediaManager has time to fetch/setup assets.
#if defined(DEBUG_PURGEWAIT_TIMEOUT)
    m_processSleep.WaitMSec(1000 * 10);
#else
    m_processSleep.WaitMSec(PURGEWAIT_TIMEOUTMS);
#endif
    m_processSleep.Reset();

    UpdateMargins();
    if (m_canPurge)
    {
      if (m_freeMBs <= m_loFreeMBs)
      {
        CLog::Log(LOGDEBUG, "**NW** - CNWPurgeManager::Process starting purge, %d MBs less than %d MBs",
          m_freeMBs, m_loFreeMBs);

        while(true)
        {
          if (!PurgeLastAccessed())
            break;

          if (m_freeMBs >= m_hiFreeMBs)
          {
            CLog::Log(LOGDEBUG, "**NW** - CNWPurgeManager::Process purge complete, %d MBs greater than %d MBs",
              m_freeMBs, m_hiFreeMBs);
            break;
          }
        }
      }
    }
  }

  #if ENABLE_NWMPURGEMANAGER_DEBUGLOGS
  CLog::Log(LOGDEBUG, "**NW** - CNWPurgeManager::Process Stopped");
  #endif
}

void CNWPurgeManager::UpdateMargins()
{
  CSingleLock lock(m_paths_lock);
  if (!m_media_paths.empty())
  {
    std::string storage_path;
    storage_path = m_media_paths[0];

    struct statfs fsInfo;
    if (statfs(CSpecialProtocol::TranslatePath(storage_path).c_str(), &fsInfo) == 0)
    {
      int64_t free = (int64_t)fsInfo.f_bfree * fsInfo.f_bsize;
      int64_t capacity = (int64_t)fsInfo.f_blocks * fsInfo.f_bsize;

      // switch units from bytes to megabytes
      free /= 1024 * 1024;
      capacity /= 1024 * 1024;

      // ignore if less than 4GB filesystem.
      if (capacity > (4 * 1024))
      {
        m_freeMBs = free;
        // less than LO_FREE triggers purge (5 percent)
        m_loFreeMBs = capacity * 0.05;
        // greater then HI_FREE ends purge (15 percent)
        m_hiFreeMBs = capacity * 0.15;

        CLog::Log(LOGDEBUG, "**NW** - CNWPurgeManager::UpdateMargins free = %d, lo_trigger = %d, hi_trigger = %d",
          m_freeMBs, m_loFreeMBs, m_hiFreeMBs);

        m_canPurge = true;
      }
    }
  }
}

bool CNWPurgeManager::PurgeLastAccessed()
{
  bool rtn = false;
  std::vector<FILEINFO> mediainfos;
  std::vector<FILEINFO> thumbinfos;

  if (!m_media_paths.empty())
  {
    CSingleLock lock(m_paths_lock);
    for (auto mediapath = m_media_paths.begin(); mediapath != m_media_paths.end(); ++mediapath)
    {
      // need last access time which means we need to manually stat each file
      // and remember st_atime as we only get back st_mtime.
      CFileItemList files;
      int flags = XFILE::DIR_FLAG_NO_FILE_DIRS | XFILE::DIR_FLAG_NO_FILE_INFO;
      XFILE::CDirectory::GetDirectory(*mediapath, files, "", flags);
      for (int i = 0; i < files.Size(); ++i)
      {
        // we still get directories even passing extension filter, WTF ?
        if (!files[i]->m_bIsFolder)
        {
          std::string filepath = files[i]->GetPath();
          std::string filename = URIUtils::GetFileName(files[i]->GetPath());
          struct __stat64 statinfo = {0};
          if (XFILE::CFile::Stat(filepath, &statinfo) == 0)
          {
            FILEINFO fileinfo;
            fileinfo.name = filename;
            fileinfo.path = filepath;

            fileinfo.ctime = statinfo.st_ctime;

            // default mtime to create time
            fileinfo.mtime = fileinfo.ctime;
            if (statinfo.st_mtime > 0)
              fileinfo.mtime = statinfo.st_mtime;

            // default atime to modify time
            fileinfo.atime = fileinfo.mtime;
            if (statinfo.st_atime > 0)
              fileinfo.atime = statinfo.st_atime;

            mediainfos.push_back(fileinfo);
          }
        }
      }
    }

    for (auto thumbpath = m_thumb_paths.begin(); thumbpath != m_thumb_paths.end(); ++thumbpath)
    {
      // need last access time which means we need to manually stat each file
      // and remember st_atime as we only get back st_mtime.
      CFileItemList files;
      int flags = XFILE::DIR_FLAG_NO_FILE_DIRS | XFILE::DIR_FLAG_NO_FILE_INFO;
      XFILE::CDirectory::GetDirectory(*thumbpath, files, "", flags);
      for (int i = 0; i < files.Size(); ++i)
      {
        // we still get directories even passing extension filter, WTF ?
        if (!files[i]->m_bIsFolder)
        {
          std::string filepath = files[i]->GetPath();
          std::string filename = URIUtils::GetFileName(filepath);
          struct __stat64 statinfo = {0};
          if (XFILE::CFile::Stat(filepath, &statinfo) == 0)
          {
            FILEINFO fileinfo;
            fileinfo.name = filename;
            fileinfo.path = filepath;

            fileinfo.ctime = statinfo.st_ctime;

            // default mtime to create time
            fileinfo.mtime = fileinfo.ctime;
            if (statinfo.st_mtime > 0)
              fileinfo.mtime = statinfo.st_mtime;

            // default atime to modify time
            fileinfo.atime = fileinfo.mtime;
            if (statinfo.st_atime > 0)
              fileinfo.atime = statinfo.st_atime;

            thumbinfos.push_back(fileinfo);
          }
        }
      }
    }
  }
  
  if (!mediainfos.empty())
  {
    // sort to last access time
    std::sort(mediainfos.begin(), mediainfos.end(),
      [] (FILEINFO const& a, FILEINFO const& b)
      {
        return a.atime < b.atime;
      });

    auto mediainfo = mediainfos.begin();
    // paranoia, if mediainfos is not empty, there is always a valid begin :)
    if (mediainfo != mediainfos.end())
    {
      std::string media_basename = mediainfo->name;
      URIUtils::RemoveExtension(media_basename);

      // search and remove any thumbnail associated with this media file
      for (auto thumbinfo = thumbinfos.begin(); thumbinfo != thumbinfos.end(); ++thumbinfo)
      {
        std::string thumb_basename = thumbinfo->name;
        URIUtils::RemoveExtension(thumb_basename);
        if (thumb_basename == media_basename)
        {
#if !defined(DEBUG_NO_DELETEFILES)
          XFILE::CFile::Delete(thumbinfo->path);
#endif
          break;
        }
      }
      CLog::Log(LOGDEBUG, "**NW** - CNWPurgeManager::PurgeLastAccessed %s at %s",
        mediainfo->name.c_str(), mediainfo->atime.GetAsDBDateTime().c_str());
#if !defined(DEBUG_NO_DELETEFILES)
      XFILE::CFile::Delete(mediainfo->path);
#endif
      rtn = true;
    }
  }

  #if ENABLE_NWMPURGEMANAGER_DEBUGLOGS
  if (!rtn)
    CLog::Log(LOGDEBUG, "**NW** - CNWPurgeManager::PurgeLastAccessed nothing to purge");
  #endif

  return rtn;
}
