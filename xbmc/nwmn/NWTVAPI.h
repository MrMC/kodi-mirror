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

#include "string"
#include "vector"

#include "XBDateTime.h"

// https://www.nationwidemember.com/tv-api-doc/

#define ENABLE_TVAPI_DEBUGLOGS 0

//const std::string kTVAPI_URLBASE = "https://api.iot-dev.envoi.cloud/tv-api/1/";
const std::string kTVAPI_URLBASE = "http://www.nationwidemember.com/tv-api/1/";
const std::string kTVAPI_URLBASE_TESTSITE = "http://test.nationwidemember.com/tv-api/1/";
//const std::string kTVAPI_URLBASEENVOI = "https://api.iot-dev.envoi.cloud/tv-api/3/";
const std::string kTVAPI_URLBASEENVOI = "https://8x4rpewbtf.execute-api.us-east-1.amazonaws.com/latest/";
const std::string kTVAPI_URLENVOITOKEN = "55de82c571202c21601dbd58a3b745c8e619d5e585fb8c3422fc0527cad82937c70dc197cadbc31e80de743aba1094c7ac11a6b4463fe025ecaeace74766d824e9192e98a5b88dfafeb0e5defda85800dc9ecebf9aa3958782cfdd8903dd8c7ca84696faf03d39d5b24ac72186efb94ea0748613204cdf7006115acd07d31dd2";

const std::string kTVAPI_UPDATEURL = "https://cdn.iot-dev.envoi.cloud/json/update.json";

const std::string kTVAPI_M3U8URL = "https://dfcdde9919f04fce8a4361271a42b5e7.7wzuvg.channel-assembly.mediatailor.us-east-1.amazonaws.com/v1/channel/envoi-channel-mediasigntv-5004/index.m3u8";

// ---------------------------------------------
// ---------------------------------------------
typedef struct TVAPI_Activate {
  std::string code;
  std::string application_id;
  // reply
  std::string apiKey;
  std::string apiSecret;
} TVAPI_Activate;

// ---------------------------------------------
// ---------------------------------------------
typedef struct TVAPI_Status {
  std::string apiKey;
  std::string apiSecret;
  // reply
  std::string key;
  std::string unique_id;  // application_id used when doing activation
  std::string machine_id;
  std::string status;
  std::string status_text;
  std::string activation_date;
} TVAPI_Status;

// ---------------------------------------------
// ---------------------------------------------
typedef struct TVAPI_MachineLocation {
    std::string id;
    std::string name;
    std::string address;
    std::string address2;
    std::string city;
    std::string state;
    std::string phone;
    std::string fax;
} TVAPI_MachineLocation;

typedef struct TVAPI_MachineNetwork {
    std::string macaddress;
    std::string macaddress_wireless;
    std::string dhcp;
    std::string ipaddress;
    std::string subnet;
    std::string router;
    std::string dns_1;
    std::string dns_2;
} TVAPI_MachineNetwork;

typedef struct TVAPI_MachineSettings {
  std::string network;
  std::string pairing;
  std::string about;
  std::string hdmibrightness;
  std::string tvresolution;
  std::string updatesoftware;
  std::string language;
  std::string legal;
} TVAPI_MachineSettings;

typedef struct TVAPI_MachineMenu {
  std::string membernettv;
  std::string vendorcommercials;
  std::string hdcontent;
  std::string membercommercials;
  std::string movietrailers;
  std::string promotionalcampaigns;
  std::string nationwidebroadcasts;
  std::string imaginationwidehd;
  std::string primemediacommercialfactory;
} TVAPI_MachineMenu;

typedef struct TVAPI_MachineMembernet_software {
  std::string id;
  std::string version;
  std::string cfbundleversion;
  std::string url;
} TVAPI_MachineMembernet_software;

typedef struct TVAPI_MachineApple_software {
  std::string id;
  std::string version;
  std::string url;
} TVAPI_MachineApple_software;

typedef struct TVAPI_Machine {
  std::string apiKey;
  std::string apiSecret;
  // reply
  std::string id;
  std::string member;
  std::string machine_name;
  std::string description;
  std::string playlist_id;
  std::string status;         //Status of the machine: ‘off’, ‘on’, ‘restarting’, ‘playing’
  std::string vendor;
  std::string hardware;
  std::string timezone;
  std::string serial_number;
  std::string warranty_number;
  std::string video_format;
  std::string allow_new_content;
  std::string allow_software_update;
  std::string update_time;
  std::string update_interval;
  TVAPI_MachineLocation location;
  TVAPI_MachineNetwork  network;
  TVAPI_MachineSettings settings;
  TVAPI_MachineMenu     menu;
  TVAPI_MachineMembernet_software  membernet_software;
  TVAPI_MachineApple_software apple_software;
} TVAPI_Machine;

const std::string kTVAPI_Status_On = "on";
const std::string kTVAPI_Status_Off = "off";
const std::string kTVAPI_Status_Playing = "playing";
const std::string kTVAPI_Status_Restarting = "restarting";

typedef struct TVAPI_MachineUpdate {
  std::string apiKey;
  std::string apiSecret;
  // post
  std::string playlist_id;
  std::string name;
  std::string description;
  std::string serial_number;
  std::string warranty_number;
  std::string macaddress;
  std::string macaddress_wireless;
  std::string vendor;
  std::string hardware_version;
  std::string timezone;
  std::string status;
  std::string allow_new_content;
  std::string allow_software_update;
  std::string update_interval;
  std::string update_time;
} TVAPI_MachineUpdate;

// ---------------------------------------------
// ---------------------------------------------
typedef struct TVAPI_PlaylistFile {
  std::string type;
  std::string path;
  std::string size;
  std::string width;
  std::string height;
  std::string etag;
  std::string mime_type;
  std::string created_date;
  std::string updated_date;
  bool        stream;
} TVAPI_PlaylistFile;

typedef struct TVAPI_PlaylistItem {
  std::string id;
  std::string name;
  std::string tv_category_id;
  std::string description;
  std::string created_date;
  std::string updated_date;
  std::string completion_date;
  std::string theatricalrelease;
  std::string dvdrelease;
  std::string download;
  std::string availability_to;
  std::string availability_from;
  TVAPI_PlaylistFile thumb;
  TVAPI_PlaylistFile poster;
  std::vector<TVAPI_PlaylistFile> files;
} TVAPI_PlaylistItem;

typedef struct TVAPI_PlaylistItems {
  std::string apiKey;
  std::string apiSecret;
  // reply
  std::string id;
  std::string name;
  std::string type;
  std::vector<TVAPI_PlaylistItem> items;
} TVAPI_PlaylistItems;

// ---------------------------------------------
// ---------------------------------------------
typedef struct TVAPI_PlaylistInfo {
  std::string id;
  std::string name;
  std::string type;
  std::string updated_date;
  std::string layout;  // horizontal or vertical
  std::string member_id;
  std::string member_name;
  std::string nmg_managed;
} TVAPI_PlaylistInfo;

typedef struct TVAPI_Playlists {
  std::string apiKey;
  std::string apiSecret;
  // reply
  //std::string page;
  //std::string perPage;
  //std::string total;
  //std::string total_pages;
  std::vector<TVAPI_PlaylistInfo> playlists;
} TVAPI_Playlists;

// ---------------------------------------------
// ---------------------------------------------
typedef struct TVAPI_CategoryId {
  std::string id;
  std::string name;
} TVAPI_CategoryId;

typedef struct TVAPI_FileId {
  std::string id;
} TVAPI_FileId;

typedef struct TVAPI_Playlist {
  std::string apiKey;
  std::string apiSecret;
  // reply
  std::string id;
  std::string name;
  std::string type;
  std::string layout;
  std::string member_id;
  std::string nmg_managed;
  std::string updated_date;
  std::vector<TVAPI_FileId> files;
  std::vector<TVAPI_CategoryId> categories;
} TVAPI_Playlist;

typedef struct TVAPI_File {
  std::string id;
  std::string date;
  std::string format;
} TVAPI_File;

typedef struct TVAPI_Files {
  std::string apiKey;
  std::string apiSecret;
  // post
  std::vector<TVAPI_File> files;
} TVAPI_Files;

typedef struct TVAPI_HealthReport {
  std::string apiKey;
  std::string apiSecret;
  // post
  std::string date;
  std::string uptime;
  std::string disk_used;
  std::string disk_free;
  std::string smart_status;
} TVAPI_HealthReport;

const std::string kTVAPI_ActionDownloaded = "downloaded";
const std::string kTVAPI_ActionHealth = "health";
const std::string kTVAPI_ActionFilePlayed = "file-played";
const std::string kTVAPI_ActionRestart = "restart";
const std::string kTVAPI_ActionPlay = "play";
const std::string kTVAPI_ActionStop = "stop";
const std::string kTVAPI_ActionSwitchOff = "switch-off";

const std::string kTVAPI_ActionStatusPending = "pending";
const std::string kTVAPI_ActionStatusInProgress = "in-progress";
const std::string kTVAPI_ActionStatusCompleted = "completed";
const std::string kTVAPI_ActionStatusError = "error";

typedef struct TVAPI_Action {
  std::string id;
  std::string action;
  std::string name;
  std::string status;
  std::string created_date;
} TVAPI_Action;

typedef struct TVAPI_Actions {
  std::string apiKey;
  std::string apiSecret;
  // get
  std::vector<TVAPI_Action> actions;
} TVAPI_Actions;

typedef struct TVAPI_ActionStatus {
  std::string id;
  std::string apiKey;
  std::string apiSecret;
  // post
  std::string status;
  std::string message;
} TVAPI_ActionStatus;


// ---------------------------------------------
// ---------------------------------------------
const std::string TVAPI_GetURLBASE();
void TVAPI_SetURLBASE(std::string urlbase);

// activation
bool TVAPI_DoActivate(TVAPI_Activate &activate);
bool TVAPI_GetStatus(TVAPI_Status &status);

// machines
bool TVAPI_GetMachine(TVAPI_Machine &machine);
bool TVAPI_UpdateMachineInfo(TVAPI_MachineUpdate &machineUpdate);

// playlists
bool TVAPI_GetPlaylists(TVAPI_Playlists &playlists);
bool TVAPI_GetPlaylist(TVAPI_Playlist &playlist, std::string playlist_id);
bool TVAPI_GetPlaylistItems(TVAPI_PlaylistItems &playlistItems, std::string playlist_id);

// reports
bool TVAPI_ReportHealth(TVAPI_HealthReport &health);
bool TVAPI_ReportFilesPlayed(TVAPI_Files &files, std::string serial_number);
bool TVAPI_ReportFilesDeleted(TVAPI_Files &files);
bool TVAPI_ReportFilesDownloaded(TVAPI_Files &files);

// actions
bool TVAPI_GetActionQueue(TVAPI_Actions &actions);
bool TVAPI_UpdateActionStatus(TVAPI_ActionStatus &actionStatus);

int  TVAPI_GetApiVersion();
void TVAPI_SetApiVersion(int version);

