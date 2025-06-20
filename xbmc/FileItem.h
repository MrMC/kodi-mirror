/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

/*!
 \file FileItem.h
 \brief
 */

#include "LockType.h"
#include "XBDateTime.h"
#include "addons/IAddon.h"
#include "guilib/GUIListItem.h"
#include "threads/CriticalSection.h"
#include "utils/IArchivable.h"
#include "utils/ISerializable.h"
#include "utils/ISortable.h"
#include "utils/SortUtils.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace MUSIC_INFO
{
  class CMusicInfoTag;
}
class CVideoInfoTag;
class CPictureInfoTag;

namespace KODI
{
namespace GAME
{
  class CGameInfoTag;
}
}

namespace PVR
{
class CPVRChannel;
class CPVREpgInfoTag;
class CPVRRecording;
class CPVRTimerInfoTag;
}

class CAlbum;
class CArtist;
class CSong;
class CGenre;

class CURL;
class CVariant;

class CFileItemList;
class CCueDocument;
typedef std::shared_ptr<CCueDocument> CCueDocumentPtr;

class IEvent;
typedef std::shared_ptr<const IEvent> EventPtr;

/* special startoffset used to indicate that we wish to resume */
#define STARTOFFSET_RESUME (-1)

class CMediaSource;

class CBookmark;

enum EFileFolderType {
  EFILEFOLDER_TYPE_ALWAYS     = 1<<0,
  EFILEFOLDER_TYPE_ONCLICK    = 1<<1,
  EFILEFOLDER_TYPE_ONBROWSE   = 1<<2,

  EFILEFOLDER_MASK_ALL        = 0xff,
  EFILEFOLDER_MASK_ONCLICK    = EFILEFOLDER_TYPE_ALWAYS
                              | EFILEFOLDER_TYPE_ONCLICK,
  EFILEFOLDER_MASK_ONBROWSE   = EFILEFOLDER_TYPE_ALWAYS
                              | EFILEFOLDER_TYPE_ONCLICK
                              | EFILEFOLDER_TYPE_ONBROWSE,
};

/*!
  \brief Represents a file on a share
  \sa CFileItemList
  */
class CFileItem :
  public CGUIListItem, public IArchivable, public ISerializable, public ISortable
{
public:
  CFileItem(void);
  CFileItem(const CFileItem& item);
  explicit CFileItem(const CGUIListItem& item);
  explicit CFileItem(const std::string& strLabel);
  explicit CFileItem(const char* strLabel);
  CFileItem(const CURL& path, bool bIsFolder);
  CFileItem(const std::string& strPath, bool bIsFolder);
  explicit CFileItem(const CSong& song);
  CFileItem(const CSong& song, const MUSIC_INFO::CMusicInfoTag& music);
  CFileItem(const CURL &path, const CAlbum& album);
  CFileItem(const std::string &path, const CAlbum& album);
  explicit CFileItem(const CArtist& artist);
  explicit CFileItem(const CGenre& genre);
  explicit CFileItem(const MUSIC_INFO::CMusicInfoTag& music);
  explicit CFileItem(const CVideoInfoTag& movie);
  explicit CFileItem(const std::shared_ptr<PVR::CPVREpgInfoTag>& tag);
  explicit CFileItem(const std::shared_ptr<PVR::CPVRChannel>& channel);
  explicit CFileItem(const std::shared_ptr<PVR::CPVRRecording>& record);
  explicit CFileItem(const std::shared_ptr<PVR::CPVRTimerInfoTag>& timer);
  explicit CFileItem(const CMediaSource& share);
  explicit CFileItem(std::shared_ptr<const ADDON::IAddon> addonInfo);
  explicit CFileItem(const EventPtr& eventLogEntry);

  ~CFileItem(void) override;
  CGUIListItem *Clone() const override { return new CFileItem(*this); };

  const CURL GetURL() const;
  void SetURL(const CURL& url);
  bool IsURL(const CURL& url) const;
  const std::string &GetPath() const { return m_strPath; };
  void SetPath(const std::string &path) { m_strPath = path; };
  bool IsPath(const std::string& path, bool ignoreURLOptions = false) const;

  const CURL GetDynURL() const;
  void SetDynURL(const CURL& url);
  const std::string &GetDynPath() const;
  void SetDynPath(const std::string &path);

  /*! \brief reset class to it's default values as per construction.
   Free's all allocated memory.
   \sa Initialize
   */
  void Reset();
  CFileItem& operator=(const CFileItem& item);
  void Archive(CArchive& ar) override;
  void Serialize(CVariant& value) const override;
  void ToSortable(SortItem &sortable, Field field) const override;
  void ToSortable(SortItem &sortable, const Fields &fields) const;
  bool IsFileItem() const override { return true; };

  bool Exists(bool bUseCache = true) const;

  /*!
   \brief Check whether an item is an optical media folder or its parent.
    This will return the non-empty path to the playable entry point of the media
    one or two levels down (VIDEO_TS.IFO for DVDs or index.bdmv for BDs).
    The returned path will be empty if folder does not meet this criterion.
   \return non-empty string if item is optical media folder, empty otherwise.
   */
  std::string GetOpticalMediaPath() const;
  /*!
   \brief Check whether an item is a video item. Note that this returns true for
    anything with a video info tag, so that may include eg. folders.
   \return true if item is video, false otherwise.
   */
  bool IsVideo() const;

  bool IsDiscStub() const;

  /*!
   \brief Check whether an item is a picture item. Note that this returns true for
    anything with a picture info tag, so that may include eg. folders.
   \return true if item is picture, false otherwise.
   */
  bool IsPicture() const;
  bool IsLyrics() const;
  bool IsSubtitle() const;

  /*!
   \brief Check whether an item is an audio item. Note that this returns true for
    anything with a music info tag, so that may include eg. folders.
   \return true if item is audio, false otherwise.
   */
  bool IsAudio() const;

  /*!
   \brief Check whether an item is 'deleted' (for example, a trashed pvr recording).
   \return true if item is 'deleted', false otherwise.
   */
  bool IsDeleted() const;

  /*!
   \brief Check whether an item is an audio book item.
   \return true if item is audiobook, false otherwise.
   */
  bool IsAudioBook() const;

  bool IsGame() const;
  bool IsCUESheet() const;
  bool IsInternetStream(const bool bStrictCheck = false) const;
  bool IsPlayList() const;
  bool IsSmartPlayList() const;
  bool IsLibraryFolder() const;
  bool IsPythonScript() const;
  bool IsPlugin() const;
  bool IsScript() const;
  bool IsAddonsPath() const;
  bool IsSourcesPath() const;
  bool IsNFO() const;
  bool IsDiscImage() const;
  bool IsOpticalMediaFile() const;
  bool IsDVDFile(bool bVobs = true, bool bIfos = true) const;
  bool IsBDFile() const;
  bool IsBluray() const;
  bool IsProtectedBlurayDisc() const;
  bool IsRAR() const;
  bool IsAPK() const;
  bool IsZIP() const;
  bool IsCBZ() const;
  bool IsCBR() const;
  bool IsISO9660() const;
  bool IsCDDA() const;
  bool IsDVD() const;
  bool IsOnDVD() const;
  bool IsOnLAN() const;
  bool IsHD() const;
  bool IsNfs() const;
  bool IsRemote() const;
  bool IsSmb() const;
  bool IsURL() const;
  bool IsStack() const;
  bool IsMultiPath() const;
  bool IsMusicDb() const;
  bool IsVideoDb() const;
  bool IsEPG() const;
  bool IsPVRChannel() const;
  bool IsPVRChannelGroup() const;
  bool IsPVRRecording() const;
  bool IsUsablePVRRecording() const;
  bool IsDeletedPVRRecording() const;
  bool IsInProgressPVRRecording() const;
  bool IsPVRTimer() const;
  bool IsType(const char *ext) const;
  bool IsVirtualDirectoryRoot() const;
  bool IsReadOnly() const;
  bool CanQueue() const;
  void SetCanQueue(bool bYesNo);
  bool IsParentFolder() const;
  bool IsFileFolder(EFileFolderType types = EFILEFOLDER_MASK_ALL) const;
  bool IsRemovable() const;
  bool IsPVR() const;
  bool IsLiveTV() const;
  bool IsRSS() const;
  bool IsMembernet() const;
  bool IsAndroidApp() const;

  void RemoveExtension();
  void CleanString();
  void FillInDefaultIcon();
  void SetFileSizeLabel();
  void SetLabel(const std::string &strLabel) override;
  int GetVideoContentType() const; /* return VIDEODB_CONTENT_TYPE, but don't want to include videodb in this header */
  bool IsLabelPreformatted() const { return m_bLabelPreformatted; }
  void SetLabelPreformatted(bool bYesNo) { m_bLabelPreformatted=bYesNo; }
  bool SortsOnTop() const { return m_specialSort == SortSpecialOnTop; }
  bool SortsOnBottom() const { return m_specialSort == SortSpecialOnBottom; }
  void SetSpecialSort(SortSpecial sort) { m_specialSort = sort; }

  inline bool HasMusicInfoTag() const
  {
    return m_musicInfoTag != NULL;
  }

  MUSIC_INFO::CMusicInfoTag* GetMusicInfoTag();

  inline const MUSIC_INFO::CMusicInfoTag* GetMusicInfoTag() const
  {
    return m_musicInfoTag;
  }

  bool HasVideoInfoTag() const;

  CVideoInfoTag* GetVideoInfoTag();

  const CVideoInfoTag* GetVideoInfoTag() const;

  inline bool HasEPGInfoTag() const
  {
    return m_epgInfoTag.get() != NULL;
  }

  inline const std::shared_ptr<PVR::CPVREpgInfoTag> GetEPGInfoTag() const
  {
    return m_epgInfoTag;
  }

  inline void SetEPGInfoTag(const std::shared_ptr<PVR::CPVREpgInfoTag>& tag)
  {
    m_epgInfoTag = tag;
  }

  inline bool HasPVRChannelInfoTag() const
  {
    return m_pvrChannelInfoTag.get() != NULL;
  }

  inline const std::shared_ptr<PVR::CPVRChannel> GetPVRChannelInfoTag() const
  {
    return m_pvrChannelInfoTag;
  }

  inline bool HasPVRRecordingInfoTag() const
  {
    return m_pvrRecordingInfoTag.get() != NULL;
  }

  inline const std::shared_ptr<PVR::CPVRRecording> GetPVRRecordingInfoTag() const
  {
    return m_pvrRecordingInfoTag;
  }

  inline bool HasPVRTimerInfoTag() const
  {
    return m_pvrTimerInfoTag != NULL;
  }

  inline const std::shared_ptr<PVR::CPVRTimerInfoTag> GetPVRTimerInfoTag() const
  {
    return m_pvrTimerInfoTag;
  }

  /*!
   \brief return the item to play. will be almost 'this', but can be different (e.g. "Play recording" from PVR EPG grid window)
   \return the item to play
   */
  CFileItem GetItemToPlay() const;

  /*!
   \brief Test if this item has a valid resume point set.
   \return True if this item has a resume point and it is set, false otherwise.
   */
  bool IsResumePointSet() const;

  /*!
   \brief Return the current resume time.
   \return The time in seconds from the start to resume playing from.
   */
  double GetCurrentResumeTime() const;

  /*!
   \brief Return the current resume time and part.
   \param startOffset will be filled with the resume time offset in seconds if item has a resume point set, is unchanged otherwise
   \param partNumber will be filled with the part number if item has a resume point set, is unchanged otherwise
   \return True if the item has a resume point set, false otherwise.
   */
  bool GetCurrentResumeTimeAndPartNumber(int64_t& startOffset, int& partNumber) const;

  inline bool HasPictureInfoTag() const
  {
    return m_pictureInfoTag != NULL;
  }

  inline const CPictureInfoTag* GetPictureInfoTag() const
  {
    return m_pictureInfoTag;
  }

  bool HasAddonInfo() const { return m_addonInfo != nullptr; }
  const std::shared_ptr<const ADDON::IAddon> GetAddonInfo() const { return m_addonInfo; }

  inline bool HasGameInfoTag() const
  {
    return m_gameInfoTag != NULL;
  }

  KODI::GAME::CGameInfoTag* GetGameInfoTag();

  inline const KODI::GAME::CGameInfoTag* GetGameInfoTag() const
  {
    return m_gameInfoTag;
  }

  CPictureInfoTag* GetPictureInfoTag();

  /*!
   \brief Get the local fanart for this item if it exists
   \return path to the local fanart for this item, or empty if none exists
   \sa GetFolderThumb, GetTBNFile
   */
  std::string GetLocalFanart() const;

  /*!
   \brief Assemble the base filename of local artwork for an item,
   accounting for archives, stacks and multi-paths, and BDMV/VIDEO_TS folders.
   `useFolder` is set to false
   \return the path to the base filename for artwork lookup.
   \sa GetLocalArt
   */
  std::string GetLocalArtBaseFilename() const;
  /*!
   \brief Assemble the base filename of local artwork for an item,
   accounting for archives, stacks and multi-paths, and BDMV/VIDEO_TS folders.
   \param useFolder whether to look in the folder for the art file. Defaults to false.
   \return the path to the base filename for artwork lookup.
   \sa GetLocalArt
   */
  std::string GetLocalArtBaseFilename(bool& useFolder) const;

  /*! \brief Assemble the filename of a particular piece of local artwork for an item.
             No file existence check is typically performed.
   \param artFile the art file to search for.
   \param useFolder whether to look in the folder for the art file. Defaults to false.
   \return the path to the local artwork.
   \sa FindLocalArt
   */
  std::string GetLocalArt(const std::string& artFile, bool useFolder = false) const;

  /*! \brief Assemble the filename of a particular piece of local artwork for an item,
             and check for file existence.
   \param artFile the art file to search for.
   \param useFolder whether to look in the folder for the art file. Defaults to false.
   \return the path to the local artwork if it exists, empty otherwise.
   \sa GetLocalArt
   */
  std::string FindLocalArt(const std::string &artFile, bool useFolder) const;

  /*! \brief Whether or not to skip searching for local art.
   \return true if local art should be skipped for this item, false otherwise.
   \sa GetLocalArt, FindLocalArt
   */
  bool SkipLocalArt() const;

  // Gets the .tbn file associated with this item
  std::string GetTBNFile() const;
  // Gets the folder image associated with this item (defaults to folder.jpg)
  std::string GetFolderThumb(const std::string &folderJPG = "folder.jpg") const;
  // Gets the correct movie title
  std::string GetMovieName(bool bUseFolderNames = false) const;

  /*! \brief Find the base movie path (i.e. the item the user expects us to use to lookup the movie)
   For folder items, with "use foldernames for lookups" it returns the folder.
   Regardless of settings, for VIDEO_TS/BDMV it returns the parent of the VIDEO_TS/BDMV folder (if present)

   \param useFolderNames whether we're using foldernames for lookups
   \return the base movie folder
   */
  std::string GetBaseMoviePath(bool useFolderNames) const;

  // Gets the user thumb, if it exists
  std::string GetUserMusicThumb(bool alwaysCheckRemote = false, bool fallbackToFolder = false) const;

  /*! \brief Get the path where we expect local metadata to reside.
   For a folder, this is just the existing path (eg tvshow folder)
   For a file, this is the parent path, with exceptions made for VIDEO_TS and BDMV files

   Three cases are handled:

     /foo/bar/movie_name/file_name          -> /foo/bar/movie_name/
     /foo/bar/movie_name/VIDEO_TS/file_name -> /foo/bar/movie_name/
     /foo/bar/movie_name/BDMV/file_name     -> /foo/bar/movie_name/

     \sa URIUtils::GetParentPath
   */
  std::string GetLocalMetadataPath() const;

  // finds a matching local trailer file
  std::string FindTrailer() const;

  virtual bool LoadMusicTag();
  virtual bool LoadGameTag();

  /* Returns the content type of this item if known */
  const std::string& GetMimeType() const { return m_mimetype; }

  /* sets the mime-type if known beforehand */
  void SetMimeType(const std::string& mimetype) { m_mimetype = mimetype; } ;

  /*! \brief Resolve the MIME type based on file extension or a web lookup
   If m_mimetype is already set (non-empty), this function has no effect. For
   http:// and shout:// streams, this will query the stream (blocking operation).
   Set lookup=false to skip any internet lookups and always return immediately.
   */
  void FillInMimeType(bool lookup = true);

  /*!
  \brief Some sources do not support HTTP HEAD request to determine i.e. mime type
  \return false if HEAD requests have to be avoided
  */
  bool ContentLookup() { return m_doContentLookup; };

  /*!
   \brief (Re)set the mime-type for internet files if allowed (m_doContentLookup)
   Some sources do not support HTTP HEAD request to determine i.e. mime type
   */
  void SetMimeTypeForInternetFile();

  /*!
   *\brief Lookup via HTTP HEAD request might not be needed, use this setter to
   * disable ContentLookup.
   */
  void SetContentLookup(bool enable) { m_doContentLookup = enable; };

  /* general extra info about the contents of the item, not for display */
  void SetExtraInfo(const std::string& info) { m_extrainfo = info; };
  const std::string& GetExtraInfo() const { return m_extrainfo; };

  /*! \brief Update an item with information from another item
   We take metadata information from the given item and supplement the current item
   with that info.  If tags exist in the new item we use the entire tag information.
   Properties are appended, and labels, thumbnail and icon are updated if non-empty
   in the given item.
   \param item the item used to supplement information
   \param replaceLabels whether to replace labels (defaults to true)
   */
  void UpdateInfo(const CFileItem &item, bool replaceLabels = true);

  bool IsSamePath(const CFileItem *item) const;

  bool IsAlbum() const;

  /*! \brief Sets details using the information from the CVideoInfoTag object
   Sets the videoinfotag and uses its information to set the label and path.
   \param video video details to use and set
   */
  void SetFromVideoInfoTag(const CVideoInfoTag &video);

  /*! \brief Sets details using the information from the CMusicInfoTag object
  Sets the musicinfotag and uses its information to set the label and path.
  \param music music details to use and set
  */
  void SetFromMusicInfoTag(const MUSIC_INFO::CMusicInfoTag &music);

  /*! \brief Sets details using the information from the CAlbum object
   Sets the album in the music info tag and uses its information to set the
   label and album-specific properties.
   \param album album details to use and set
   */
  void SetFromAlbum(const CAlbum &album);
  /*! \brief Sets details using the information from the CSong object
   Sets the song in the music info tag and uses its information to set the
   label, path, song-specific properties and artwork.
   \param song song details to use and set
   */
  void SetFromSong(const CSong &song);

  bool m_bIsShareOrDrive;    ///< is this a root share/drive
  int m_iDriveType;     ///< If \e m_bIsShareOrDrive is \e true, use to get the share type. Types see: CMediaSource::m_iDriveType
  CDateTime m_dateTime;             ///< file creation date & time
  int64_t m_dwSize;             ///< file size (0 for folders)
  std::string m_strDVDLabel;
  std::string m_strTitle;
  int m_iprogramCount;
  int m_idepth;
  int64_t m_lStartOffset;
  int m_lStartPartNumber;
  int64_t m_lEndOffset;
  LockType m_iLockMode;
  std::string m_strLockCode;
  int m_iHasLock; // 0 - no lock 1 - lock, but unlocked 2 - locked
  int m_iBadPwdCount;

  void SetCueDocument(const CCueDocumentPtr& cuePtr);
  void LoadEmbeddedCue();
  bool HasCueDocument() const;
  bool LoadTracksFromCueDocument(CFileItemList& scannedItems);
private:
  /*! \brief initialize all members of this class (not CGUIListItem members) to default values.
   Called from constructors, and from Reset()
   \sa Reset, CGUIListItem
   */
  void Initialize();

  /*!
   \brief Return the current resume point for this item.
   \return The resume point.
   */
  CBookmark GetResumePoint() const;

  /*!
   \brief If given channel is radio, fill item's music tag from given epg tag and channel info.
   */
  void FillMusicInfoTag(const std::shared_ptr<PVR::CPVRChannel>& channel, const std::shared_ptr<PVR::CPVREpgInfoTag>& tag);

  std::string m_strPath;            ///< complete path to item
  std::string m_strDynPath;

  SortSpecial m_specialSort;
  bool m_bIsParentFolder;
  bool m_bCanQueue;
  bool m_bLabelPreformatted;
  std::string m_mimetype;
  std::string m_extrainfo;
  bool m_doContentLookup;
  MUSIC_INFO::CMusicInfoTag* m_musicInfoTag;
  CVideoInfoTag* m_videoInfoTag;
  std::shared_ptr<PVR::CPVREpgInfoTag> m_epgInfoTag;
  std::shared_ptr<PVR::CPVRChannel> m_pvrChannelInfoTag;
  std::shared_ptr<PVR::CPVRRecording> m_pvrRecordingInfoTag;
  std::shared_ptr<PVR::CPVRTimerInfoTag> m_pvrTimerInfoTag;
  CPictureInfoTag* m_pictureInfoTag;
  std::shared_ptr<const ADDON::IAddon> m_addonInfo;
  KODI::GAME::CGameInfoTag* m_gameInfoTag;
  EventPtr m_eventLogEntry;
  bool m_bIsAlbum;

  CCueDocumentPtr m_cueDocument;
};

/*!
  \brief A shared pointer to CFileItem
  \sa CFileItem
  */
typedef std::shared_ptr<CFileItem> CFileItemPtr;

/*!
  \brief A vector of pointer to CFileItem
  \sa CFileItem
  */
typedef std::vector< CFileItemPtr > VECFILEITEMS;

/*!
  \brief Iterator for VECFILEITEMS
  \sa CFileItemList
  */
typedef std::vector< CFileItemPtr >::iterator IVECFILEITEMS;

/*!
  \brief A map of pointers to CFileItem
  \sa CFileItem
  */
typedef std::map<std::string, CFileItemPtr > MAPFILEITEMS;

/*!
  \brief Iterator for MAPFILEITEMS
  \sa MAPFILEITEMS
  */
typedef std::map<std::string, CFileItemPtr >::iterator IMAPFILEITEMS;

/*!
  \brief Pair for MAPFILEITEMS
  \sa MAPFILEITEMS
  */
typedef std::pair<std::string, CFileItemPtr > MAPFILEITEMSPAIR;

typedef bool (*FILEITEMLISTCOMPARISONFUNC) (const CFileItemPtr &pItem1, const CFileItemPtr &pItem2);
typedef void (*FILEITEMFILLFUNC) (CFileItemPtr &item);

/*!
  \brief Represents a list of files
  \sa CFileItemList, CFileItem
  */
class CFileItemList : public CFileItem
{
public:
  enum CACHE_TYPE { CACHE_NEVER = 0, CACHE_IF_SLOW, CACHE_ALWAYS };

  CFileItemList();
  explicit CFileItemList(const std::string& strPath);
  ~CFileItemList() override;
  void Archive(CArchive& ar) override;
  CFileItemPtr operator[] (int iItem);
  const CFileItemPtr operator[] (int iItem) const;
  CFileItemPtr operator[] (const std::string& strPath);
  const CFileItemPtr operator[] (const std::string& strPath) const;
  void Clear();
  void ClearItems();
  void Add(CFileItemPtr item);
  void Add(CFileItem&& item);
  void AddFront(const CFileItemPtr &pItem, int itemPosition);
  void Remove(CFileItem* pItem);
  void Remove(int iItem);
  CFileItemPtr Get(int iItem);
  const CFileItemPtr Get(int iItem) const;
  const VECFILEITEMS& GetList() const { return m_items; }
  CFileItemPtr Get(const std::string& strPath);
  const CFileItemPtr Get(const std::string& strPath) const;
  int Size() const;
  bool IsEmpty() const;
  void Append(const CFileItemList& itemlist);
  void Assign(const CFileItemList& itemlist, bool append = false);
  bool Copy  (const CFileItemList& item, bool copyItems = true);
  void Reserve(int iCount);
  void Sort(SortBy sortBy, SortOrder sortOrder, SortAttribute sortAttributes = SortAttributeNone);
  /* \brief Sorts the items based on the given sorting options

  In contrast to Sort (see above) this does not change the internal
  state by storing the sorting method and order used and therefore
  will always execute the sorting even if the list of items has
  already been sorted with the same options before.
  */
  void Sort(SortDescription sortDescription);
  void Randomize();
  void FillInDefaultIcons();
  int GetFolderCount() const;
  int GetFileCount() const;
  int GetSelectedCount() const;
  int GetObjectCount() const;
  void FilterCueItems();
  void RemoveExtensions();
  void SetIgnoreURLOptions(bool ignoreURLOptions);
  void SetFastLookup(bool fastLookup);
  bool Contains(const std::string& fileName) const;
  bool GetFastLookup() const { return m_fastLookup; };

  /*! \brief stack a CFileItemList
   By default we stack all items (files and folders) in a CFileItemList
   \param stackFiles whether to stack all items or just collapse folders (defaults to true)
   \sa StackFiles,StackFolders
   */
  void Stack(bool stackFiles = true);

  SortOrder GetSortOrder() const { return m_sortDescription.sortOrder; }
  SortBy GetSortMethod() const { return m_sortDescription.sortBy; }
  void SetSortOrder(SortOrder sortOrder) { m_sortDescription.sortOrder = sortOrder; }
  void SetSortMethod(SortBy sortBy) { m_sortDescription.sortBy = sortBy; }

  /*! \brief load a CFileItemList out of the cache

   The file list may be cached based on which window we're viewing in, as different
   windows will be listing different portions of the same URL (eg viewing music files
   versus viewing video files)

   \param windowID id of the window that's loading this list (defaults to 0)
   \return true if we loaded from the cache, false otherwise.
   \sa Save,RemoveDiscCache
   */
  bool Load(int windowID = 0);

  /*! \brief save a CFileItemList to the cache

   The file list may be cached based on which window we're viewing in, as different
   windows will be listing different portions of the same URL (eg viewing music files
   versus viewing video files)

   \param windowID id of the window that's saving this list (defaults to 0)
   \return true if successful, false otherwise.
   \sa Load,RemoveDiscCache
   */
  bool Save(int windowID = 0);
  void SetCacheToDisc(CACHE_TYPE cacheToDisc) { m_cacheToDisc = cacheToDisc; }
  bool CacheToDiscAlways() const { return m_cacheToDisc == CACHE_ALWAYS; }
  bool CacheToDiscIfSlow() const { return m_cacheToDisc == CACHE_IF_SLOW; }
  /*! \brief remove a previously cached CFileItemList from the cache

   The file list may be cached based on which window we're viewing in, as different
   windows will be listing different portions of the same URL (eg viewing music files
   versus viewing video files)

   \param windowID id of the window whose cache we which to remove (defaults to 0)
   \sa Save,Load
   */
  void RemoveDiscCache(int windowID = 0) const;
  void RemoveDiscCache(const std::string& cachefile) const;
  void RemoveDiscCacheCRC(const std::string& crc) const;
  bool AlwaysCache() const;

  void Swap(unsigned int item1, unsigned int item2);

  /*! \brief Update an item in the item list
   \param item the new item, which we match based on path to an existing item in the list
   \return true if the item exists in the list (and was thus updated), false otherwise.
   */
  bool UpdateItem(const CFileItem *item);

  void AddSortMethod(SortBy sortBy, int buttonLabel, const LABEL_MASKS &labelMasks, SortAttribute sortAttributes = SortAttributeNone);
  void AddSortMethod(SortBy sortBy, SortAttribute sortAttributes, int buttonLabel, const LABEL_MASKS &labelMasks);
  void AddSortMethod(SortDescription sortDescription, int buttonLabel, const LABEL_MASKS &labelMasks);
  bool HasSortDetails() const { return m_sortDetails.size() != 0; }
  const std::vector<GUIViewSortDetails> &GetSortDetails() const { return m_sortDetails; }

  /*! \brief Specify whether this list should be sorted with folders separate from files
   By default we sort with folders listed (and sorted separately) except for those sort modes
   which should be explicitly sorted with folders interleaved with files (eg SORT_METHOD_FILES).
   With this set the folder state will be ignored, allowing folders and files to sort interleaved.
   \param sort whether to ignore the folder state.
   */
  void SetSortIgnoreFolders(bool sort) { m_sortIgnoreFolders = sort; };
  bool GetReplaceListing() const { return m_replaceListing; };
  void SetReplaceListing(bool replace);
  void SetContent(const std::string &content) { m_content = content; };
  const std::string &GetContent() const { return m_content; };

  void ClearSortState();

  VECFILEITEMS::iterator begin() { return m_items.begin(); }
  VECFILEITEMS::iterator end() { return m_items.end(); }
  VECFILEITEMS::const_iterator begin() const { return m_items.begin(); }
  VECFILEITEMS::const_iterator end() const { return m_items.end(); }
  VECFILEITEMS::const_iterator cbegin() const { return m_items.cbegin(); }
  VECFILEITEMS::const_iterator cend() const { return m_items.cend(); }
private:
  void Sort(FILEITEMLISTCOMPARISONFUNC func);
  void FillSortFields(FILEITEMFILLFUNC func);
  std::string GetDiscFileCache(int windowID) const;

  /*!
   \brief stack files in a CFileItemList
   \sa Stack
   */
  void StackFiles();

  /*!
   \brief stack folders in a CFileItemList
   \sa Stack
   */
  void StackFolders();

  VECFILEITEMS m_items;
  MAPFILEITEMS m_map;
  bool m_ignoreURLOptions = false;
  bool m_fastLookup = false;
  SortDescription m_sortDescription;
  bool m_sortIgnoreFolders = false;
  CACHE_TYPE m_cacheToDisc = CACHE_IF_SLOW;
  bool m_replaceListing = false;
  std::string m_content;

  std::vector<GUIViewSortDetails> m_sortDetails;

  mutable CCriticalSection m_lock;
};
