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

#include "system.h"

#include "NWIoT.h"
#include "NWClient.h"

#include "Application.h"
#include "network/Network.h"
#include "messaging/ApplicationMessenger.h"
#include "Util.h"
#include "ServiceBroker.h"
//#include "guilib/GUIComponent.h"
//#include "URL.h"
#include "dialogs/GUIDialogKaiToast.h"
//#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#include "filesystem/SpecialProtocol.h"
//#include "guilib/GUIWindowManager.h"
#include "interfaces/AnnouncementManager.h"
//#include "network/Network.h"
#include "settings/Settings.h"
//#include "utils/FileUtils.h"
#include "utils/log.h"
//#include "utils/TimeUtils.h"
//#include "utils/StringUtils.h"
#include "utils/SystemInfo.h"
#include "utils/URIUtils.h"
//#include "utils/XBMCTinyXML.h"
//#include "utils/XMLUtils.h"

//#include "guilib/GUIKeyboardFactory.h"
//#include "guilib/GUIWindowManager.h"
//#include "settings/MediaSourceSettings.h"
//#include "storage/MediaManager.h"
#include "filesystem/SpecialProtocol.h"

#include "settings/SettingsComponent.h"
#include "utils/XTimeUtils.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "speedtest/SpeedTest.h"


#include <string>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <thread>

// AWS IoT below
#include <aws/crt/Api.h>
#include <aws/crt/JsonObject.h>
#include <aws/crt/UUID.h>
#include <aws/crt/io/HostResolver.h>

#include <aws/iot/MqttClient.h>

#include <aws/iotidentity/CreateCertificateFromCsrRequest.h>
#include <aws/iotidentity/CreateCertificateFromCsrResponse.h>
#include <aws/iotidentity/CreateCertificateFromCsrSubscriptionRequest.h>
#include <aws/iotidentity/CreateKeysAndCertificateRequest.h>
#include <aws/iotidentity/CreateKeysAndCertificateResponse.h>
#include <aws/iotidentity/CreateKeysAndCertificateSubscriptionRequest.h>
#include <aws/iotidentity/ErrorResponse.h>
#include <aws/iotidentity/IotIdentityClient.h>
#include <aws/iotidentity/RegisterThingRequest.h>
#include <aws/iotidentity/RegisterThingResponse.h>
#include <aws/iotidentity/RegisterThingSubscriptionRequest.h>

#include <aws/iotshadow/ErrorResponse.h>
#include <aws/iotshadow/IotShadowClient.h>
#include <aws/iotshadow/ShadowDeltaUpdatedEvent.h>
#include <aws/iotshadow/ShadowDeltaUpdatedSubscriptionRequest.h>
#include <aws/iotshadow/UpdateShadowRequest.h>
#include <aws/iotshadow/UpdateShadowResponse.h>
#include <aws/iotshadow/UpdateShadowSubscriptionRequest.h>

//static std::string strEndPoint = "a1l40foo64a71s-ats.iot.us-east-2.amazonaws.com";

static std::string strEndPoint = "a2xxj2lq07qxu1-ats.iot.us-east-1.amazonaws.com";

using namespace Aws::Crt;
//using namespace Aws::Iotidentity;
using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono;      // nanoseconds, system_clock, seconds
//using namespace Aws::Iotshadow;

static const char *SHADOW_STATUS_VALUE_DEFAULT = "online";
static const char *SHADOW_PLAYBACK_VALUE_DEFAULT = "stop";
static const char *SHADOW_ORIENTATION_VALUE_DEFAULT = "horizontal";
static const bool SHADOW_REBOOT_VALUE_DEFAULT = false;
static const bool SHADOW_QUIT_VALUE_DEFAULT = false;

CCriticalSection CNWIoT::m_payloadLock;

std::string strCAPath;
std::string strCertPath;
std::string strPrivatePath;
std::string strProvisionedKeyPath;
std::string strProvisionedCrtPath;
String strThingName;
std::shared_ptr<Mqtt::MqttConnection> connection;

CVariant    location;
int         sequence;

static void s_changeShadowValue(
    Aws::Iotshadow::IotShadowClient &client,
    const String &thingName,
    const String &shadowProperty,
    const String &value,
    bool resetDesired = true)
{
    CLog::Log(LOGINFO,  "Changing local shadow value to %s.\n", value.c_str());

    Aws::Iotshadow::ShadowState state;
    JsonObject desired;
    desired.WithString(shadowProperty, value);
    JsonObject reported;
    reported.WithString(shadowProperty, value);
    if (resetDesired)
      state.Desired = desired;
    state.Reported = reported;

    Aws::Iotshadow::UpdateShadowRequest updateShadowRequest;
    std::string playerMACAddress = GetNUCMACAddress();
    String uuid(playerMACAddress.c_str());
    updateShadowRequest.ClientToken = uuid;
    updateShadowRequest.ThingName = thingName;
    updateShadowRequest.State = state;

    auto publishCompleted = [thingName, value](int ioErr) {
        if (ioErr != AWS_OP_SUCCESS)
        {
            CLog::Log(LOGINFO,  "failed to update %s shadow state: error %s\n", thingName.c_str(), ErrorDebugString(ioErr));
            return;
        }

        CLog::Log(LOGINFO,  "Successfully updated shadow state for %s, to %s\n", thingName.c_str(), value.c_str());
    };

    client.PublishUpdateShadow(updateShadowRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, std::move(publishCompleted));
}

static void i_changeShadowValue(
    Aws::Iotshadow::IotShadowClient &client,
    const String &thingName,
    const String &shadowProperty,
    const uint8_t &value,
    bool resetDesired = true)
{
    CLog::Log(LOGINFO,  "Changing local shadow value to %i.\n", value);

    Aws::Iotshadow::ShadowState state;
    JsonObject desired;
    desired.WithInt64(shadowProperty, value);
    JsonObject reported;
    reported.WithInt64(shadowProperty, value);
    if (resetDesired)
      state.Desired = desired;
    state.Reported = reported;

    Aws::Iotshadow::UpdateShadowRequest updateShadowRequest;
    std::string playerMACAddress = GetNUCMACAddress();
    String uuid(playerMACAddress.c_str());
    updateShadowRequest.ClientToken = uuid;
    updateShadowRequest.ThingName = thingName;
    updateShadowRequest.State = state;

    auto publishCompleted = [thingName, value](int ioErr) {
        if (ioErr != AWS_OP_SUCCESS)
        {
            CLog::Log(LOGINFO,  "failed to update %s shadow state: error %s\n", thingName.c_str(), ErrorDebugString(ioErr));
            return;
        }

        CLog::Log(LOGINFO,  "Successfully updated shadow state for %s, to %i\n", thingName.c_str(), value);
    };

    client.PublishUpdateShadow(updateShadowRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, std::move(publishCompleted));
}

static void b_changeShadowValue(
    Aws::Iotshadow::IotShadowClient &client,
    const String &thingName,
    const String &shadowProperty,
    const bool &value,
    bool resetDesired = true)
{
    CLog::Log(LOGINFO,  "Changing local shadow value to %s.\n", value ? "true":"false");

    Aws::Iotshadow::ShadowState state;
    JsonObject desired;
    desired.WithBool(shadowProperty, value);
    JsonObject reported;
    reported.WithBool(shadowProperty, value);
    if (resetDesired)
      state.Desired = desired;
    state.Reported = reported;

    Aws::Iotshadow::UpdateShadowRequest updateShadowRequest;
    std::string playerMACAddress = GetNUCMACAddress();
    String uuid(playerMACAddress.c_str());
    updateShadowRequest.ClientToken = uuid;
    updateShadowRequest.ThingName = thingName;
    updateShadowRequest.State = state;

    auto publishCompleted = [thingName, value](int ioErr) {
        if (ioErr != AWS_OP_SUCCESS)
        {
            CLog::Log(LOGINFO,  "failed to update %s shadow state: error %s\n", thingName.c_str(), ErrorDebugString(ioErr));
            return;
        }

        CLog::Log(LOGINFO,  "Successfully updated shadow state for %s, to %s\n", thingName.c_str(), value ? "true":"false");
    };

    client.PublishUpdateShadow(updateShadowRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, std::move(publishCompleted));
}

CNWIoT::CNWIoT()
: CThread("CNWIoT")
{
  CLog::Log(LOGDEBUG, "**NW** - NW version %s", CSysInfo::GetVersionShort());
  CServiceBroker::GetAnnouncementManager()->AddAnnouncer(this);
  strProvisionedKeyPath = CSpecialProtocol::TranslatePath("special://home/nwmn/" + kNWClient_CertPath + "NW_private.pem.key");
  strProvisionedCrtPath = CSpecialProtocol::TranslatePath("special://home/nwmn/" + kNWClient_CertPath + "NW_certificate.pem.crt");
  strPrivatePath        = CSpecialProtocol::TranslatePath("special://xbmc/system/" + kNWClient_CertPath + "provision-private.pem.key");
  strCertPath           = CSpecialProtocol::TranslatePath("special://xbmc/system/" + kNWClient_CertPath + "provision-certificate.pem.crt");
  strCAPath = CSpecialProtocol::TranslatePath("special://xbmc/system/" + kNWClient_CertPath + "root-CA.crt");
  CLog::Log(LOGINFO, "**MN** - CNWIoT::CNWIoT() - dump provisioned %s %s", strProvisionedCrtPath, strProvisionedKeyPath);
  CLog::Log(LOGINFO, "**MN** - CNWIoT::CNWIoT() - dump private %s %s", strPrivatePath, strCertPath);
  m_heartbeatTimer.StartZero();
  sequence = 0;
}

CNWIoT::~CNWIoT()
{
  CServiceBroker::GetAnnouncementManager()->RemoveAnnouncer(this);
  StopThread();
}

void CNWIoT::StopIoT()
{
  m_bStop = true;
  StopThread();
}

CNWIoT& CNWIoT::GetInstance()
{
  static CNWIoT IoT;
  return IoT;
}

void CNWIoT::Announce(ANNOUNCEMENT::AnnouncementFlag flag, const std::string &sender, const std::string &message, const CVariant &data)
{
  if (sender != "xbmc")
    return;

  if (flag == ANNOUNCEMENT::Player || flag == ANNOUNCEMENT::Other)
  {
    if (message == "OnPlay")
    {
      #if ENABLE_NWIOT_DEBUGLOGS
      CLog::Log(LOGDEBUG, "**MN** - CNWIoT::Announce() - Playback started");
      #endif
      CFileItem currentFile(g_application.CurrentFileItem());
      if (currentFile.GetProperty("Membernet").asBoolean())
      {
        std::string assetID = currentFile.GetProperty("assetId").asString();;
        std::string format = currentFile.GetProperty("video_format").asString();
        CDateTime time = CDateTime::GetCurrentDateTime();
        std::string payload = StringUtils::Format("%s,%s,%s",
          time.GetAsDBDateTime().c_str(),
          assetID.c_str(),
          format.c_str()
        );
        // { machineId: ‘’, eventType: ‘’, details: { } }
        CVariant payloadObject;
        payloadObject["details"]["assetId"] = assetID;
        payloadObject["details"]["assetGroupID"] =currentFile.GetProperty("assetGroupID").asString();
        payloadObject["details"]["raw"] = payload;
        notifyEvent("playbackStart", payloadObject);
      }
    }
    else if (message == "OnStop")
    {
      if (data.isMember("end") && data["end"] == false)
      {
        #if ENABLE_NWIOT_DEBUGLOGS
        CLog::Log(LOGINFO, "**MN** - CNWIoT::Announce() - Playback stopped");
        #endif
        std::string assetID = data["assetID"].asString();
        std::string format = data["format"].asString();
        CDateTime time = CDateTime::GetCurrentDateTime();
        std::string payload = StringUtils::Format("%s,%s,%s",
          time.GetAsDBDateTime().c_str(),
          assetID.c_str(),
          format.c_str()
        );
        /*
        {
          "machineId": "98:01:A7:90:8C:BF",
          "timestamp": "2020-10-21 21:33:31",
          "type": "playbackStart",
          "details": { "assetId": "7B81E1D4-C8BF-4B98-B651-F1951BFB0456", "raw": "2020-10-21 21:33:31,7B81E1D4-C8BF-4B98-B651-F1951BFB0456" }
        }
        */
        CVariant payloadObject;
        payloadObject["details"]["assetId"] = assetID;
        payloadObject["details"]["assetGroupID"] = data["assetGroupID"].asString();
        payloadObject["details"]["raw"] = payload;
        notifyEvent("playbackStop", payloadObject);
      }
    }
    else if (message == "MNmsg")
    {
      if (data.isMember("msg"))
      {
        if (data["msg"] == "authorise")
        {
          StopThread();
          CLog::Log(LOGINFO, "**MN** - CNWIoT::Announce() - authorise");
          DoAuthorize();
          Listen();
        }
        if (data["msg"] == "about")
        {
          // { machineId: ‘’, eventType: ‘’, details: { } }
          CVariant payloadObject;
          payloadObject["details"] = data["payload"].asString();
          payloadObject["details"]["raw"] = data["payload"].asString();
          notifyEvent("about", payloadObject);
        }
      }
    }
    else if (message == "MNgotPlaylist")
    {
      CVariant payloadObject;
      payloadObject["details"] = "";
      notifyEvent("playlistReceived", payloadObject);
    }
    else if (message == "MNassetDownloaded")
    {
      CVariant payloadObject;
      payloadObject["details"]["assetID"] = data["assetID"].asString();
      payloadObject["details"]["raw"] = data["payload"].asString();
      notifyEvent("assetDownloaded", payloadObject);
    }
  }
}

void CNWIoT::MsgReceived(CVariant msgPayload)
{

  /*
  {
   "message": {
      "details": {
        "orientation": "vertical",
        "reportState": "true",
        "reportStats": "true"
      },
      "machineId": "98:01:A7:90:8C:BF",
      "type": "machineAction"
     }
  }
  */
  if (!msgPayload.isNull())
  {
    std::string playerMACAddress = GetNUCMACAddress();
    if (msgPayload["type"].asString() == "machineAction" && msgPayload["machineId"] == playerMACAddress)
    {
      CVariant msgDetails = msgPayload["details"];
      if (!msgDetails.isNull())
      {
        if (msgDetails.isMember("orientation"))
        {
          if (msgDetails["orientation"].asString() == "vertical")
            CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::MN_VERTICAL, true);
          else
            CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::MN_VERTICAL, false);
          CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
        }
        if (msgDetails.isMember("reportState"))
        {
          // here we send back the machine state...
        }
        if (msgDetails.isMember("reportStats") && msgDetails["reportStats"].asBoolean())
        {
          CNWClient* client = CNWClient::GetClient();
          NWPlayerInfo playerInfo;
          client->GetPlayerInfo(playerInfo);
          std::string payload;
          payload = playerInfo.macaddress + "\n";
          payload += playerInfo.serial_number + "\n";
          CVariant payloadObject;
          payloadObject["details"] = payload;
          payloadObject["details"]["raw"] = payload;
          notifyEvent("reportStats", payloadObject);
        }
        if (msgDetails.isMember("startPlayback") && msgDetails["startPlayback"].asBoolean())
        {
          // start playback
          CNWClient* client = CNWClient::GetClient();
          if (client->IsAuthorized())
          {
            client->Startup(false, false);
          }
        }
        if (msgDetails.isMember("stopPlayback") && msgDetails["stopPlayback"].asBoolean())
        {
          // Stop Playback
          CNWClient* client = CNWClient::GetClient();
          if (client->IsAuthorized())
          {
            client->StopPlaying();
          }
        }
        if (msgDetails.isMember("notify"))
        {
          // Send toast msg
          CGUIDialogKaiToast::QueueNotification("Message from the backend", msgDetails["notify"].asString());
        }
        if (msgDetails.isMember("quit") && msgDetails["quit"].asBoolean())
        {
          // quit the app
          KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
          CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - Quit");
        }
        if (msgDetails.isMember("reboot") && msgDetails["reboot"].asBoolean())
        {
          // reboot the machine
          // disabled on OSX
#ifndef TARGET_DARWIN
          KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_RESTART);
#endif
          CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - Reboot");
        }
        if (msgDetails.isMember("speedtest") && msgDetails["speedtest"].asBoolean())
        {
          auto sp = SpeedTest(SPEED_TEST_MIN_SERVER_VERSION);
          IPInfo info;
          CVariant payloadObject;
          ServerInfo serverInfo;
          ServerInfo serverQualityInfo;

          if (sp.ipInfo(info))
          {
            // Timestamp
            CDateTime time = CDateTime::GetCurrentDateTime();
            payloadObject["timestamp"] = time.GetAsDBDateTime().c_str();

            // IP info
            payloadObject["client"]["ip"] = info.ip_address;
            payloadObject["client"]["lat"] = info.lat;
            payloadObject["client"]["lon"] = info.lon;
            payloadObject["client"]["isp"] = info.isp;

            // servers
            auto serverList = sp.serverList();
            if (!serverList.empty())
            {
              payloadObject["servers_online"] = (int)serverList.size();
              int sampleSize = 5;
              serverInfo = sp.bestServer(sampleSize);

              payloadObject["server"]["name"]     = serverInfo.name;
              payloadObject["server"]["distance"] = serverInfo.distance;
              payloadObject["server"]["latency"]  = (double)sp.latency();
              payloadObject["server"]["host"]     = serverInfo.host;
              payloadObject["server"]["country"]  = serverInfo.country;
              payloadObject["server"]["id"]       = serverInfo.id;

              // general
              payloadObject["ping"]["latency"] = (double)sp.latency();
              long jitter = 0;
              if (sp.jitter(serverInfo, jitter))
              {
                payloadObject["ping"]["jitter"] = (double)jitter;
              }

              // precheck test
              double preSpeed = 0;
              if (sp.downloadSpeed(serverInfo, preflightConfigDownload, preSpeed))
              {
                TestConfig uploadConfig;
                TestConfig downloadConfig;

                if (preSpeed > 4 && preSpeed <= 30)
                {
                    downloadConfig = narrowConfigDownload;
                    uploadConfig   = narrowConfigUpload;
                }
                else if (preSpeed > 30 && preSpeed < 150)
                {
                    downloadConfig = broadbandConfigDownload;
                    uploadConfig   = broadbandConfigUpload;
                }
                else if (preSpeed >= 150)
                {
                    downloadConfig = fiberConfigDownload;
                    uploadConfig   = fiberConfigUpload;
                }

                // download test
                double downloadSpeed = 0;
                CStopWatch  timer;
                timer.StartZero();
                if (sp.downloadSpeed(serverInfo, downloadConfig, downloadSpeed))
                {
                  payloadObject["download"]["bytes"] = (int)(downloadSpeed*1000*1000);
                  int elapsed = timer.GetElapsedMilliseconds();
                  payloadObject["download"]["elapsed"] = elapsed;
                }

                // upload test
                double uploadSpeed = 0;
                timer.StartZero();
                if (sp.uploadSpeed(serverInfo, uploadConfig, uploadSpeed))
                {
                  payloadObject["upload"]["bytes"] = (int)(uploadSpeed*1000*1000);
                  int elapsed = timer.GetElapsedMilliseconds();
                  payloadObject["upload"]["elapsed"] = elapsed;
                }
              }
            }
          }
          CVariant payload;
          payload["details"] = payloadObject;
          notifyEvent("speedTest", payload);
        }
      }
    }
  }
  else
    CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - Did not have 'message' json object");
}

bool CNWIoT::DoAuthorize()
{
  ApiHandle apiHandle;

  String token;
  String endpoint;
  String certificatePath;
  std::string playerMACAddress = GetNUCMACAddress();
  String clientId(playerMACAddress.c_str());
  String templateName = "MN";
  std::string strTemplateParameters = "{\"SerialNumber\":\"" + playerMACAddress + "\"}";
  CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Serial %s", strTemplateParameters);
  String templateParameters(strTemplateParameters.c_str(), strTemplateParameters.size());

  String keyContent;
  String certificateContent;

  Aws::Iotidentity::RegisterThingResponse registerThingResponse;
  apiHandle.InitializeLogging(Aws::Crt::LogLevel::None, stderr);

  std::promise<bool> connectionCompletedPromise;
  std::promise<void> connectionClosedPromise;

  auto onConnectionCompleted = [&](Mqtt::MqttConnection &, int errorCode, Mqtt::ReturnCode returnCode, bool)
  {
    if (errorCode)
    {
      CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Connection failed with error %s", ErrorDebugString(errorCode));
      connectionCompletedPromise.set_value(false);
    }
    else
    {
      CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Connection okay with %d",returnCode);
      connectionCompletedPromise.set_value(true);
    }
  };

  /*
   * Invoked when a disconnect message has completed.
   */
  auto onDisconnect = [&](Mqtt::MqttConnection & /*conn*/)
  {
    {
      CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Disconnect completed");
      connectionClosedPromise.set_value();
    }
  };

  Io::EventLoopGroup eventLoopGroup(2);
  if (!eventLoopGroup)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Event Loop Group Creation failed with error %s",ErrorDebugString(eventLoopGroup.LastError()));
    return false;
  }

  Io::DefaultHostResolver hostResolver(eventLoopGroup, 3, 30);
  Io::ClientBootstrap bootstrap(eventLoopGroup, hostResolver);

  if (!bootstrap)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - ClientBootstrap failed with error %s", ErrorDebugString(bootstrap.LastError()));
    return false;
  }

  auto clientConfigBuilder = Aws::Iot::MqttClientConnectionConfigBuilder(strCertPath.c_str(),strPrivatePath.c_str());

  clientConfigBuilder.WithEndpoint(strEndPoint.c_str());
  if (!strCAPath.empty())
  {
      clientConfigBuilder.WithCertificateAuthority(strCAPath.c_str());
  }
  auto clientConfig = clientConfigBuilder.Build();

  if (!clientConfig)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Client Configuration initialization failed with error %s",ErrorDebugString(clientConfig.LastError()));
    return false;
  }

  Aws::Iot::MqttClient mqttClient(bootstrap);

  /*
   * Since no exceptions are used, always check the bool operator
   * when an error could have occurred.
   */
  if (!mqttClient)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - MQTT Client Creation failed with error %s", ErrorDebugString(mqttClient.LastError()));
    return false;
  }
  auto connection = mqttClient.NewConnection(clientConfig);

  if (!*connection)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - MQTT Connection Creation failed with error %s", ErrorDebugString(connection->LastError()));
    return false;
  }

  connection->OnConnectionCompleted = std::move(onConnectionCompleted);
  connection->OnDisconnect = std::move(onDisconnect);

  /*
   * Actually perform the connect dance.
   */
  CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Connecting...");
  if (!connection->Connect(clientId.c_str(), true, 0))
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - MQTT Connection failed with error %s", ErrorDebugString(connection->LastError()));
    return false;
  }

  if (connectionCompletedPromise.get_future().get())
  {
    Aws::Iotidentity::IotIdentityClient identityClient(connection);

    std::promise<void> keysPublishCompletedPromise;
    std::promise<void> keysAcceptedCompletedPromise;
    std::promise<void> keysRejectedCompletedPromise;

    std::promise<void> registerPublishCompletedPromise;
    std::promise<void> registerAcceptedCompletedPromise;
    std::promise<void> registerRejectedCompletedPromise;

    auto onKeysPublishSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error publishing to CreateKeysAndCertificate: %s", ErrorDebugString(ioErr));
       }
       keysPublishCompletedPromise.set_value();
    };

    auto onKeysAcceptedSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error subscribing to CreateKeysAndCertificate accepted: %s", ErrorDebugString(ioErr));
       }
       keysAcceptedCompletedPromise.set_value();
    };

    auto onKeysRejectedSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error subscribing to CreateKeysAndCertificate rejected: %s", ErrorDebugString(ioErr));
       }
       keysRejectedCompletedPromise.set_value();
    };

    auto onKeysAccepted = [&](Aws::Iotidentity::CreateKeysAndCertificateResponse *response, int ioErr)
    {
       if (ioErr == AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - CreateKeysAndCertificateResponse certificateId: %s.", response->CertificateId->c_str());
         token = *response->CertificateOwnershipToken;
         certificateContent = *response->CertificatePem;
         keyContent = *response->PrivateKey;
       }
       else
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error on subscription: %s.", ErrorDebugString(ioErr));
       }
    };

    auto onKeysRejected = [&](Aws::Iotidentity::ErrorResponse *error, int ioErr)
    {
       if (ioErr == AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - CreateKeysAndCertificate failed with statusCode %d, errorMessage %s and errorCode %s.",
               *error->StatusCode,
               error->ErrorMessage->c_str(),
               error->ErrorCode->c_str());
       }
       else
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error on subscription: %s.", ErrorDebugString(ioErr));
       }
    };

    auto onRegisterAcceptedSubAck = [&](int ioErr) {
       if (ioErr != AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error subscribing to RegisterThing accepted: %s", ErrorDebugString(ioErr));
       }
       registerAcceptedCompletedPromise.set_value();
    };

    auto onRegisterRejectedSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error subscribing to RegisterThing rejected: %s", ErrorDebugString(ioErr));
       }
       registerRejectedCompletedPromise.set_value();
    };

    auto onRegisterAccepted = [&](Aws::Iotidentity::RegisterThingResponse *response, int ioErr)
    {
       if (ioErr == AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - RegisterThingResponse ThingName: %s.", response->ThingName->c_str());
       }
       else
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error on subscription: %s.", ErrorDebugString(ioErr));
       }
    };

    auto onRegisterRejected = [&](Aws::Iotidentity::ErrorResponse *error, int ioErr)
    {
       if (ioErr == AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - RegisterThing failed with statusCode %d, errorMessage %s and errorCode %s.",
               *error->StatusCode,
               error->ErrorMessage->c_str(),
               error->ErrorCode->c_str());
       }
       else
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error on subscription: %s.", ErrorDebugString(ioErr));
       }
    };

    auto onRegisterPublishSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
         CLog::Log(LOGINFO, "**MN** - CNWIoT::DoAuthorize() - Error publishing to RegisterThing: %s", ErrorDebugString(ioErr));
           exit(-1);
       }

       registerPublishCompletedPromise.set_value();
    };

    // CreateKeysAndCertificate workflow
    Aws::Iotidentity::CreateKeysAndCertificateSubscriptionRequest keySubscriptionRequest;
    identityClient.SubscribeToCreateKeysAndCertificateAccepted(
        keySubscriptionRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onKeysAccepted, onKeysAcceptedSubAck);

    identityClient.SubscribeToCreateKeysAndCertificateRejected(
        keySubscriptionRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onKeysRejected, onKeysRejectedSubAck);

    Aws::Iotidentity::CreateKeysAndCertificateRequest createKeysAndCertificateRequest;
    identityClient.PublishCreateKeysAndCertificate(
        createKeysAndCertificateRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onKeysPublishSubAck);

    Aws::Iotidentity::RegisterThingSubscriptionRequest registerSubscriptionRequest;
    registerSubscriptionRequest.TemplateName = templateName;

    identityClient.SubscribeToRegisterThingAccepted(
        registerSubscriptionRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onRegisterAccepted, onRegisterAcceptedSubAck);

    identityClient.SubscribeToRegisterThingRejected(
        registerSubscriptionRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onRegisterRejected, onRegisterRejectedSubAck);

    Sleep(1000);

    Aws::Iotidentity::RegisterThingRequest registerThingRequest;
    registerThingRequest.TemplateName = templateName;

    const Aws::Crt::String jsonValue = templateParameters;
    Aws::Crt::JsonObject value(jsonValue);
    Map<String, JsonView> pm = value.View().GetAllObjects();
    Aws::Crt::Map<Aws::Crt::String, Aws::Crt::String> params =
        Aws::Crt::Map<Aws::Crt::String, Aws::Crt::String>();

    for (const auto &x : pm)
    {
        params.emplace(x.first, x.second.AsString());
    }

    registerThingRequest.Parameters = params;
    registerThingRequest.CertificateOwnershipToken = token;

    identityClient.PublishRegisterThing(
        registerThingRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onRegisterPublishSubAck);
    Sleep(1000);

    keysPublishCompletedPromise.get_future().wait();
    keysAcceptedCompletedPromise.get_future().wait();
    keysRejectedCompletedPromise.get_future().wait();
    registerPublishCompletedPromise.get_future().wait();
    registerAcceptedCompletedPromise.get_future().wait();
    registerRejectedCompletedPromise.get_future().wait();
  }
  /* Disconnect */
  CLog::Log(LOGDEBUG, "**NW** - 1-connection->Disconnect()");

  if (connection->Disconnect())
  {
      connectionClosedPromise.get_future().wait();
  }

  // save provisioned keys for later
  if (!certificateContent.empty() && !keyContent.empty())
  {
    XFILE::CFile::Delete(strProvisionedKeyPath);
    XFILE::CFile::Delete(strProvisionedCrtPath);

    std::string strkeyContent = keyContent.c_str();
    std::string strCertificateContent = certificateContent.c_str();

    // Save privateKey
    XFILE::CFile keyFile;
    keyFile.OpenForWrite(strProvisionedKeyPath);
    keyFile.Write(strkeyContent.c_str(), strkeyContent.size());
    keyFile.Close();

    // Save Certificate
    XFILE::CFile crtFile;
    crtFile.OpenForWrite(strProvisionedCrtPath);
    crtFile.Write(strCertificateContent.c_str(), strCertificateContent.size());
    crtFile.Close();

    return true;
  }
  return false;
}

bool CNWIoT::IsAuthorized()
{
  // check if we have valid certs
  return (XFILE::CFile::Exists(strProvisionedKeyPath) &&
          XFILE::CFile::Exists(strProvisionedCrtPath));
}

void CNWIoT::Listen()
{
  Create();
//  bool didAuthorise = false;
//  if (!IsAuthorized())
//    didAuthorise = DoAuthorize();
//  else
//    didAuthorise = true;
//
//  if (didAuthorise)
//  {
//    Create();
//  }
//  else
//  {
//    CLog::Log(LOGDEBUG, "**MN** - CNWIoT::Listen() - Failed to 'DoAuthorize()' ");
//  }
}

void CNWIoT::Process()
{
  SetPriority(THREAD_PRIORITY_NORMAL);
  //#if ENABLE_NWIOT_DEBUGLOGS
  CLog::Log(LOGDEBUG, "**NW** - CNWIoT::Process Started");
  //#endif
  ApiHandle apiHandle;
  apiHandle.InitializeLogging(Aws::Crt::LogLevel::None, stderr);


  while (!CServiceBroker::GetNetwork().GetFirstConnectedInterface())
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - No Network device attached.... waiting");
    Sleep(1000);
  }

  while (!IsAuthorized())
  {
    DoAuthorize();
    Sleep(5000);
  }

  CLog::Log(LOGDEBUG, "**MN** - CNWIoT::Process() - Completed 'DoAuthorize()' ");

  std::string playerMACAddress = GetNUCMACAddress();
  std::string thingName = "MN_" + playerMACAddress;
  String aws_s(thingName.c_str(), thingName.size());
  strThingName = aws_s;

  std::string strDTopic = "dt/envoi/events/MN_" + playerMACAddress;
  std::string strCMDTopic = "cmd/envoi/events/MN_" + playerMACAddress;
  String dtopic(strDTopic.c_str(),strDTopic.size());
  String cmdtopic(strCMDTopic.c_str(),strCMDTopic.size());
  String clientId(playerMACAddress.c_str());
  std::promise<bool> connectionCompletedPromise;
  std::promise<void> connectionClosedPromise;

  bool connected = false;

      /*
   * This will execute when an mqtt connect has completed or failed.
   */
  auto onConnectionCompleted = [&](Mqtt::MqttConnection &, int errorCode, Mqtt::ReturnCode returnCode, bool) {
      if (errorCode)
      {
        CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Connection failed with error %s", ErrorDebugString(errorCode));
          connectionCompletedPromise.set_value(false);
      }
      else
      {
          if (returnCode != AWS_MQTT_CONNECT_ACCEPTED)
          {
            CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Connection failed with mqtt return code %d", (int)returnCode);
              connectionCompletedPromise.set_value(false);
          }
          else
          {
            CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Connection completed successfully.");
            connectionCompletedPromise.set_value(true);
            connected = true;
            CVariant payloadObject;
            payloadObject["details"]["mnversion"] = CSysInfo::GetVersionShort();
            notifyEvent("deviceConnected", payloadObject);
          }
      }
  };

  auto onInterrupted = [&](Mqtt::MqttConnection &, int error) {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Connection interrupted with error %s", ErrorDebugString(error));
  };

  auto onResumed = [&](Mqtt::MqttConnection &, Mqtt::ReturnCode, bool)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Connection resumed\n");

  };

  /*
   * Invoked when a disconnect message has completed.
   */
  auto onDisconnect = [&](Mqtt::MqttConnection &) {
      {
        CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Disconnect completed");
        connectionClosedPromise.set_value();
        connected = false;
      }
  };

  Io::EventLoopGroup eventLoopGroup(2);
  if (!eventLoopGroup)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Event Loop Group Creation failed with error %s", ErrorDebugString(eventLoopGroup.LastError()));
    return;
  }

  Aws::Crt::Io::DefaultHostResolver defaultHostResolver(eventLoopGroup, 3, 300);
  Io::ClientBootstrap bootstrap(eventLoopGroup, defaultHostResolver);

  if (!bootstrap)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - ClientBootstrap failed with error %s", ErrorDebugString(bootstrap.LastError()));
    return;
  }

  auto clientConfigBuilder = Aws::Iot::MqttClientConnectionConfigBuilder(strProvisionedCrtPath.c_str(),strProvisionedKeyPath.c_str());

  Aws::Iot::MqttClientConnectionConfigBuilder builder;

  if (!strProvisionedCrtPath.empty() && !strProvisionedKeyPath.empty())
  {
    builder = Aws::Iot::MqttClientConnectionConfigBuilder(strProvisionedCrtPath.c_str(), strProvisionedKeyPath.c_str());
  }

  builder.WithEndpoint(strEndPoint.c_str());
  if (!strCAPath.empty())
  {
    builder.WithCertificateAuthority(strCAPath.c_str());
  }

  auto clientConfig = builder.Build();

  if (!clientConfig)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Client Configuration initialization failed with error %s",ErrorDebugString(clientConfig.LastError()));
    return;
  }

  Aws::Iot::MqttClient mqttClient(bootstrap);

  /*
   * Since no exceptions are used, always check the bool operator
   * when an error could have occurred.
   */
  if (!mqttClient)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - MQTT Client Creation failed with error %s", ErrorDebugString(mqttClient.LastError()));
    return;
  }
  auto connection = mqttClient.NewConnection(clientConfig);

  if (!*connection)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Failed to create client");
    return;
  }

  connection->OnConnectionCompleted = std::move(onConnectionCompleted);
  connection->OnDisconnect = std::move(onDisconnect);
  connection->OnConnectionInterrupted = std::move(onInterrupted);
  connection->OnConnectionResumed = std::move(onResumed);

  while (!m_bStop && !connected)
  {
    CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Connecting...");
    if (!connection->Connect(clientId.c_str(), false, 1000))
    {
      CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - MQTT Connection failed with error %s", ErrorDebugString(connection->LastError()));
    }
    else
      connected = true;
    Sleep(1000);
  }

//  if (connectionCompletedPromise.get_future().get())
//  {
//
//  }

  Aws::Iotshadow::IotShadowClient shadowClient(connection);
  if (connectionCompletedPromise.get_future().get())
  {
    std::promise<void> subscribeDeltaCompletedPromise;
    std::promise<void> subscribeDeltaAcceptedCompletedPromise;
    std::promise<void> subscribeDeltaRejectedCompletedPromise;

    auto onDeltaUpdatedSubAck = [&](int ioErr)
    {
        if (ioErr != AWS_OP_SUCCESS)
        {
          CLog::Log(LOGINFO,  "**MN** - CNWIoT::Process() - Error subscribing to shadow delta: %s\n", ErrorDebugString(ioErr));
        }
        else
        {
          subscribeDeltaCompletedPromise.set_value();
        }
    };

    auto onDeltaUpdatedAcceptedSubAck = [&](int ioErr)
    {
        if (ioErr != AWS_OP_SUCCESS)
        {
          CLog::Log(LOGINFO,  "**MN** - CNWIoT::Process() - Error subscribing to shadow delta accepted: %s\n", ErrorDebugString(ioErr));
        }
        else
        {
          subscribeDeltaAcceptedCompletedPromise.set_value();
        }
    };

    auto onDeltaUpdatedRejectedSubAck = [&](int ioErr)
    {
        if (ioErr != AWS_OP_SUCCESS)
        {
          CLog::Log(LOGINFO,  "**MN** - CNWIoT::Process() - Error subscribing to shadow delta rejected: %s\n", ErrorDebugString(ioErr));
        }
        else
        {
          subscribeDeltaRejectedCompletedPromise.set_value();
        }
    };

    auto onDeltaUpdated = [&](Aws::Iotshadow::ShadowDeltaUpdatedEvent *event, int ioErr)
    {
        if (event)
        {
          CLog::Log(LOGINFO,  "**MN** - CNWIoT::Process() - Received shadow delta event.\n");
          if (event->State)
          {
            if (event->State->View().ValueExists("orientation"))
            {
              JsonView objectView = event->State->View().GetJsonObject("orientation");
              if (!objectView.IsNull())
              {
                  if (event->State->View().GetString("orientation") == "vertical")
                    CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::MN_VERTICAL, true);
                  else
                    CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::MN_VERTICAL, false);
                  CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
                  s_changeShadowValue(shadowClient, strThingName, "orientation", event->State->View().GetString("orientation"));
              }
              CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - Orientation - %s", event->State->View().GetString("orientation"));
            }
            if (event->State->View().ValueExists("playback"))
            {
              CNWClient* client = CNWClient::GetClient();
              if (client->IsAuthorized())
              {
                if (event->State->View().GetString("playback") == "play")
                  client->Startup(false, false);
                else if (event->State->View().GetString("playback") == "stop")
                  client->StopPlaying();

                s_changeShadowValue(shadowClient, strThingName, "playback", "");
              }
              CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - Playback - %s", event->State->View().GetString("playback"));
            }
            if (event->State->View().ValueExists("quit"))
            {
              if (event->State->View().GetBool("quit"))
              {
                b_changeShadowValue(shadowClient, strThingName, "quit", false);
                Sleep(2000);
                KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
                CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - Quit");
              }
              else
                b_changeShadowValue(shadowClient, strThingName, "quit", SHADOW_QUIT_VALUE_DEFAULT);
            }
            if (event->State->View().ValueExists("reboot"))
            {
              if (event->State->View().GetBool("reboot"))
              {
                CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - Reboot");
                b_changeShadowValue(shadowClient, strThingName, "reboot", false);
                Sleep(2000);
                // reboot the machine
                // disabled for testing on OSX
#ifndef TARGET_DARWIN
                system("/usr/bin/systemctl reboot");
#endif
              }
              else
                b_changeShadowValue(shadowClient, strThingName, "reboot", SHADOW_REBOOT_VALUE_DEFAULT);
            }
            if (event->State->View().ValueExists("forceFirmwareUpdate"))
            {
              if (event->State->View().GetBool("forceFirmwareUpdate"))
              {
                std::string updateURL;
                b_changeShadowValue(shadowClient, strThingName, "forceFirmwareUpdate", false);
                CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - forceFirmwareUpdate");
                CNWClient* client = CNWClient::GetClient();
                if (event->State->View().ValueExists("forceFirmwareUpdateURL"))
                {
                  String const& aws_s = event->State->View().GetString("forceFirmwareUpdateURL");
                  std::string s(aws_s.c_str(), aws_s.size());
                  updateURL = s;
                  s_changeShadowValue(shadowClient, strThingName, "forceFirmwareUpdateURL", "");

                }
                client->CheckUpdate(updateURL);
              }
              else
                b_changeShadowValue(shadowClient, strThingName, "forceFirmwareUpdate", false);
            }
            if (event->State->View().KeyExists("enableSSH"))
            {
              s_changeShadowValue(shadowClient, strThingName, "enableSSH", event->State->View().GetString("enableSSH"));
              std::string enableSshConf = "/storage/.cache/services/sshd.conf";
              std::string disableSshConf = "/storage/.cache/services/sshd.disabled";
#ifdef TARGET_DARWIN
              enableSshConf = CSpecialProtocol::TranslatePath("special://nwmn/sshd.conf");
              disableSshConf = CSpecialProtocol::TranslatePath("special://nwmn/sshd.disabled");
#endif

              std::string strkeyContent = "SSH_ARGS=\"\"\nSSHD_DISABLE_PW_AUTH=false\n";
              if (event->State->View().GetString("enableSSH") == "1")
              {
//                if (!XFILE::CFile::Exists(enableSshConf))
                {
                  XFILE::CFile sshFile;
                  sshFile.OpenForWrite(enableSshConf);
                  sshFile.Write(strkeyContent.c_str(), strkeyContent.size());
                  sshFile.Close();

                  XFILE::CFile::Delete(disableSshConf);
                }
              }
              else
              {
//                if (!XFILE::CFile::Exists(disableSshConf))
                {
                  XFILE::CFile sshFile;
                  sshFile.OpenForWrite(disableSshConf);
                  sshFile.Write(strkeyContent.c_str(), strkeyContent.size());
                  sshFile.Close();

                  XFILE::CFile::Delete(enableSshConf);
                }
              }
            }
            if (event->State->View().ValueExists("apiVersion"))
            {
              int apiVersion = event->State->View().GetInteger("apiVersion");
              CNWClient* client = CNWClient::GetClient();
              client->StopPlaying();
              client->SetApiVersion(apiVersion);
              client->ClearLocalPlayer();
              client->ResetStartupState();
              client->Startup(false, false);
              
              CLog::Log(LOGINFO, "**MN** - CNWIoT::MsgReceived - apiVersion requested is %i", apiVersion);

              Sleep(200);
              i_changeShadowValue(shadowClient, strThingName, "apiVersion", client->GetApiVersion());
            }
          }
        }

        if (ioErr)
        {
          CLog::Log(LOGINFO,  "**MN** - CNWIoT::Process() - Error processing shadow delta - onDeltaUpdated: %s\n", ErrorDebugString(ioErr));
        }
    };

    auto onUpdateShadowAccepted = [&](Aws::Iotshadow::UpdateShadowResponse *response, int ioErr) {
        if (ioErr != AWS_OP_SUCCESS)
        {
          CLog::Log(LOGINFO,  "**MN** - CNWIoT::Process() - Error on onUpdateShadowAccepted: %s.\n", ErrorDebugString(ioErr));
        }
    };

    auto onUpdateShadowRejected = [&](Aws::Iotshadow::ErrorResponse *error, int ioErr) {
        if (ioErr == AWS_OP_SUCCESS)
        {
          CLog::Log(LOGINFO,
                "**MN** - CNWIoT::Process() - Update of shadow state failed with message %s and code %d.",
                error->Message->c_str(),
                *error->Code);
        }
        else
        {
          CLog::Log(LOGINFO,  "**MN** - CNWIoT::Process() - Error on onUpdateShadowRejected: %s.\n", ErrorDebugString(ioErr));
        }
    };

    Aws::Iotshadow::ShadowDeltaUpdatedSubscriptionRequest shadowDeltaUpdatedRequest;
    shadowDeltaUpdatedRequest.ThingName = strThingName;

    shadowClient.SubscribeToShadowDeltaUpdatedEvents(
        shadowDeltaUpdatedRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onDeltaUpdated, onDeltaUpdatedSubAck);

    Aws::Iotshadow::UpdateShadowSubscriptionRequest updateShadowSubscriptionRequest;
    updateShadowSubscriptionRequest.ThingName = strThingName;

    shadowClient.SubscribeToUpdateShadowAccepted(
        updateShadowSubscriptionRequest,
        AWS_MQTT_QOS_AT_LEAST_ONCE,
        onUpdateShadowAccepted,
        onDeltaUpdatedAcceptedSubAck);

    shadowClient.SubscribeToUpdateShadowRejected(
        updateShadowSubscriptionRequest,
        AWS_MQTT_QOS_AT_LEAST_ONCE,
        onUpdateShadowRejected,
        onDeltaUpdatedRejectedSubAck);

    subscribeDeltaCompletedPromise.get_future().wait();
    subscribeDeltaAcceptedCompletedPromise.get_future().wait();
    subscribeDeltaRejectedCompletedPromise.get_future().wait();

    // report orientation, do not reset "desired" state
    if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::MN_VERTICAL))
      s_changeShadowValue(shadowClient, strThingName, "orientation", "vertical", false);
    else
      s_changeShadowValue(shadowClient, strThingName, "orientation", SHADOW_ORIENTATION_VALUE_DEFAULT, false);

    // reset quit and reboot to false on start, we dont want to get stuck in an infinite loop
    b_changeShadowValue(shadowClient, strThingName, "quit", SHADOW_QUIT_VALUE_DEFAULT);
    b_changeShadowValue(shadowClient, strThingName, "reboot", SHADOW_REBOOT_VALUE_DEFAULT);


    auto onPublish = [&](Mqtt::MqttConnection &, const String &topic, const ByteBuf &byteBuf)
    {
      CVariant resultObject;
      String payload((char *)byteBuf.buffer, byteBuf.len);
      std::string pl = (char *)byteBuf.buffer;
      CJSONVariantParser::Parse(pl, resultObject);
      MsgReceived(resultObject);
    };

    /*
     * Subscribe for incoming publish messages on topic.
     */
    std::promise<void> subscribeFinishedPromise;
    auto onSubAck = [&](Mqtt::MqttConnection &, uint16_t packetId, const String &topic, Mqtt::QOS QoS, int errorCode)
    {
            if (errorCode)
            {
              CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Subscribe failed with error %s", aws_error_debug_str(errorCode));
                exit(-1);
            }
            else
            {
                if (!packetId || QoS == AWS_MQTT_QOS_FAILURE)
                {
                  CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Subscribe rejected by the broker.");
                    exit(-1);
                }
                else
                {
                  CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Subscribe on topic %s on packetId %d Succeeded", topic.c_str(), packetId);
                }
            }
            subscribeFinishedPromise.set_value();
        };

      connection->Subscribe(cmdtopic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, onPublish, onSubAck);
      subscribeFinishedPromise.get_future().wait();

  }




  while (!m_bStop)
  {
    if (!CServiceBroker::GetNetwork().GetFirstConnectedInterface())
      continue;
    // report online status every 30000ms (30sec)
    if (m_heartbeatTimer.IsRunning() && m_heartbeatTimer.GetElapsedMilliseconds() > 30000.0f)
    {
      CVariant payloadObject;
      payloadObject["details"]["status"] = SHADOW_STATUS_VALUE_DEFAULT;
      notifyEvent("heartbeat", payloadObject);
      m_heartbeatTimer.StartZero();
    }

    {
      CSingleLock lock(m_payloadLock);
      if (m_payload.size() > 0)
      {
        for (std::vector<std::string>::iterator it = m_payload.begin(); it != m_payload.end(); ++it)
        {
          std::string msg = *it;
          ByteBuf payload = ByteBufNewCopy(DefaultAllocator(), (const uint8_t *)msg.c_str(), msg.length());
          ByteBuf *payloadPtr = &payload;
          auto onPublishComplete = [payloadPtr](Mqtt::MqttConnection &, uint16_t packetId, int errorCode)
          {
            aws_byte_buf_clean_up(payloadPtr);
            if (packetId)
            {
              CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Operation on packetId %d Succeeded", packetId);
            }
            else
            {
              CLog::Log(LOGINFO, "**MN** - CNWIoT::Process() - Operation failed with error %s", aws_error_debug_str(errorCode));
            }
          };
          connection->Publish(dtopic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, false, payload, onPublishComplete);
        }
        m_payload.clear();
      }
    }
  }

//   enable if we subscribe to listen to a topic
    std::promise<void> unsubscribeFinishedPromise;
    connection->Unsubscribe(
        cmdtopic.c_str(), [&](Mqtt::MqttConnection &, uint16_t, int) { unsubscribeFinishedPromise.set_value(); });
    unsubscribeFinishedPromise.get_future().wait();

  /* Disconnect */
  CLog::Log(LOGINFO, "**NW** - connection->Disconnect()");
  if (connection->Disconnect())
  {
    connectionClosedPromise.get_future().wait();
  }

  CLog::Log(LOGINFO, "**NW** - CNWIoT::Process Stopped");
}

void CNWIoT::setPayload(std::string payload)
{
  CSingleLock lock(m_payloadLock);
  m_payload.push_back(payload);
}

void CNWIoT::notifyEvent(std::string type, CVariant details)
{
  // check if we have location
  getLocation();

  CVariant payloadObject;
  CDateTime time = CDateTime::GetCurrentDateTime();
  std::string playerMACAddress = "NA";
  if (CServiceBroker::GetNetwork().GetFirstConnectedInterface())
    playerMACAddress = GetNUCMACAddress();
  payloadObject["machineId"] = playerMACAddress;
  payloadObject["type"] = type;
  payloadObject["timestamp"] = time.GetAsDBDateTime().c_str();
  payloadObject["details"] = details["details"];
  payloadObject["session"] = location["session"];
  payloadObject["sequenceNumber"] = sequence;
  std::string payloadStr;
  CJSONVariantWriter::Write(payloadObject, payloadStr, true);
  setPayload(payloadStr);
  sequence += 1;
}

void CNWIoT::getLocation()
{
  if (location.isNull())
  {
    CURL url("https://utility.envoi.cloud/reflector/iot");
    XFILE::CCurlFile curl;
    curl.SetAcceptEncoding("gzip");
    std::string strResponse;
    if (curl.Get(url.Get(), strResponse))
    {
      CLog::Log(LOGDEBUG, "CNWIoT::getLocation() %s", strResponse.c_str());
      CJSONVariantParser::Parse(strResponse, location);
    }
    std::string test;
  }
}
