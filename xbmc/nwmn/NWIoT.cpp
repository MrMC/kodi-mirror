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

#include "Application.h"
#include "network/Network.h"
//#include "messaging/ApplicationMessenger.h"
#include "Util.h"
#include "ServiceBroker.h"
//#include "guilib/GUIComponent.h"
//#include "URL.h"
//#include "dialogs/GUIDialogKaiToast.h"
//#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "filesystem/CurlFile.h"
#include "filesystem/SpecialProtocol.h"
//#include "guilib/GUIWindowManager.h"
#include "interfaces/AnnouncementManager.h"
//#include "network/Network.h"
//#include "settings/Settings.h"
//#include "utils/FileUtils.h"
#include "utils/log.h"
//#include "utils/TimeUtils.h"
//#include "utils/StringUtils.h"
//#include "utils/SystemInfo.h"
#include "utils/URIUtils.h"
//#include "utils/XBMCTinyXML.h"
//#include "utils/XMLUtils.h"

//#include "guilib/GUIKeyboardFactory.h"
//#include "guilib/GUIWindowManager.h"
//#include "settings/MediaSourceSettings.h"
//#include "storage/MediaManager.h"
#include "filesystem/SpecialProtocol.h"

#include "settings/SettingsComponent.h"
#include "platform/linux/XTimeUtils.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "dialogs/GUIDialogKaiToast.h"


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

static std::string strEndPoint = "a1l40foo64a71s-ats.iot.us-east-2.amazonaws.com";

std::string strCAPath;
std::string strCertPath;
std::string strPrivatePath;
std::string strProvisionedKeyPath;
std::string strProvisionedCrtPath;

using namespace Aws::Crt;
using namespace Aws::Iotidentity;
using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono;      // nanoseconds, system_clock, seconds

CCriticalSection CNWIoT::m_payloadLock;

CNWIoT::CNWIoT()
: CThread("CNWIoT")
{
  CLog::Log(LOGDEBUG, "**NW** - NW version %f", kNWClient_PlayerFloatVersion);
  CServiceBroker::GetAnnouncementManager()->AddAnnouncer(this);
  strProvisionedKeyPath = CSpecialProtocol::TranslatePath("special://home/nwmn/" + kNWClient_CertPath + "private.pem.key");
  strProvisionedCrtPath = CSpecialProtocol::TranslatePath("special://home/nwmn/" + kNWClient_CertPath + "certificate.pem.crt");
  strPrivatePath        = CSpecialProtocol::TranslatePath("special://xbmc/system/" + kNWClient_CertPath + "provision-private.pem.key");
  strCertPath           = CSpecialProtocol::TranslatePath("special://xbmc/system/" + kNWClient_CertPath + "provision-certificate.pem.crt");
  strCAPath = CSpecialProtocol::TranslatePath("special://xbmc/system/" + kNWClient_CertPath + "root-CA.crt");
}

CNWIoT::~CNWIoT()
{
  CServiceBroker::GetAnnouncementManager()->RemoveAnnouncer(this);
  StopThread();
}

void CNWIoT::Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data)
{
  if (strcmp(sender, "xbmc") != 0)
    return;

  if (flag == ANNOUNCEMENT::Player || flag == ANNOUNCEMENT::Other)
  {
    if (strcmp(message, "OnPlay") == 0)
    {
      #if ENABLE_NWIOT_DEBUGLOGS
      CLog::Log(LOGDEBUG, "**MN** - CNWIoT::Announce() - Playback started");
      #endif
      CFileItem currentFile(g_application.CurrentFileItem());
      std::string strPath = currentFile.GetPath();
      std::string assetID = URIUtils::GetFileName(strPath);
      URIUtils::RemoveExtension(assetID);
      std::string format = currentFile.GetProperty("video_format").asString();
      CDateTime time = CDateTime::GetCurrentDateTime();
      std::string payload = StringUtils::Format("%s,%s,%s",
        time.GetAsDBDateTime().c_str(),
        assetID.c_str(),
        format.c_str()
      );
      // { machineId: ‘’, eventType: ‘’, details: { } }
      CVariant payloadObject;
      std::string uuid = CServiceBroker::GetNetwork().GetFirstConnectedInterface()->GetMacAddress();
      payloadObject["machineId"] = uuid;
      payloadObject["type"] = "playbackStart";
      payloadObject["timestamp"] = time.GetAsDBDateTime().c_str();
      payloadObject["details"]["assetId"] = assetID;
      payloadObject["details"]["raw"] = payload;
      std::string payloadStr;
      CJSONVariantWriter::Write(payloadObject, payloadStr, false);
      CNWIoT::setPayload(payloadStr);
    }
    else if (strcmp(message, "OnStop") == 0)
    {
      if (data.isMember("end") && data["end"] == false)
      {
        #if ENABLE_NWIOT_DEBUGLOGS
        CLog::Log(LOGNOTICE, "**MN** - CNWIoT::Announce() - Playback stopped");
        #endif
        CFileItem currentFile(g_application.CurrentFileItem());
        std::string strPath = currentFile.GetPath();
        std::string assetID = URIUtils::GetFileName(strPath);
        URIUtils::RemoveExtension(assetID);
        std::string format = currentFile.GetProperty("video_format").asString();
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
        std::string uuid = CServiceBroker::GetNetwork().GetFirstConnectedInterface()->GetMacAddress();
        payloadObject["machineId"] = uuid;
        payloadObject["type"] = "playbackStop";
        payloadObject["timestamp"] = time.GetAsDBDateTime().c_str();
        payloadObject["details"]["assetId"] = assetID;
        payloadObject["details"]["raw"] = payload;
        std::string payloadStr;
        CJSONVariantWriter::Write(payloadObject, payloadStr, false);
        CNWIoT::setPayload(payloadStr);
      }
    }
    else if (strcmp(message, "MNmsg") == 0)
    {
      if (data.isMember("msg"))
      {
        if (data["msg"] == "authorise")
        {
          StopThread();
          CLog::Log(LOGNOTICE, "**MN** - CNWIoT::Announce() - authorise");
          DoAuthorize();
          Listen();
        }
        if (data["msg"] == "about")
        {
          // { machineId: ‘’, eventType: ‘’, details: { } }
          CVariant payloadObject;
          std::string uuid = CServiceBroker::GetNetwork().GetFirstConnectedInterface()->GetMacAddress();
          payloadObject["machineId"] = uuid;
          payloadObject["type"] = "about";
          payloadObject["details"] = data["payload"].asString();
          CDateTime time = CDateTime::GetCurrentDateTime();
          payloadObject["timestamp"] = time.GetAsDBDateTime().c_str();
//          payloadObject["details"]["assetId"] = assetID;
          payloadObject["details"]["raw"] = data["payload"].asString();
          std::string payloadStr;
          CJSONVariantWriter::Write(payloadObject, payloadStr, false);
          CNWIoT::setPayload(payloadStr);
        }
      }
    }
  }
}

void CNWIoT::MsgReceived(CVariant msgPayload)
{
  if (msgPayload.isMember("message"))
  {
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, "AWS IoT", msgPayload["message"].asString(), 3000, false);
    CLog::Log(LOGNOTICE, "**MN** - CNWIoT::MsgReceived - %s", msgPayload["message"].asString());
  }
  else
    CLog::Log(LOGNOTICE, "**MN** - CNWIoT::MsgReceived - Did not have 'message' json object");

}

bool CNWIoT::DoAuthorize()
{
  ApiHandle apiHandle;

  String token;
  String endpoint;
  String certificatePath;
  String clientId(Aws::Crt::UUID().ToString());
  String templateName = "MN";
  std::string uuid = CServiceBroker::GetNetwork().GetFirstConnectedInterface()->GetMacAddress();
  std::string strTemplateParameters = "{\"SerialNumber\":\"" + uuid + "\"}";
  String templateParameters(strTemplateParameters.c_str(), strTemplateParameters.size());

  String keyContent;
  String certificateContent;

  RegisterThingResponse registerThingResponse;
  apiHandle.InitializeLogging(Aws::Crt::LogLevel::Fatal, stderr);

  std::promise<bool> connectionCompletedPromise;
  std::promise<void> connectionClosedPromise;

  auto onConnectionCompleted = [&](Mqtt::MqttConnection &, int errorCode, Mqtt::ReturnCode returnCode, bool)
  {
    if (errorCode)
    {
      fprintf(stdout, "Connection failed with error %s\n", ErrorDebugString(errorCode));
      connectionCompletedPromise.set_value(false);
    }
    else
    {
      fprintf(stdout, "Connection completed with return code %d\n", returnCode);
      connectionCompletedPromise.set_value(true);
    }
  };

  /*
   * Invoked when a disconnect message has completed.
   */
  auto onDisconnect = [&](Mqtt::MqttConnection & /*conn*/)
  {
    {
      fprintf(stdout, "Disconnect completed\n");
      connectionClosedPromise.set_value();
    }
  };

  Io::EventLoopGroup eventLoopGroup(1);
  if (!eventLoopGroup)
  {
    fprintf(stderr, "Event Loop Group Creation failed with error %s\n", ErrorDebugString(eventLoopGroup.LastError()));
    return false;
  }

  Io::DefaultHostResolver hostResolver(eventLoopGroup, 2, 30);
  Io::ClientBootstrap bootstrap(eventLoopGroup, hostResolver);

  if (!bootstrap)
  {
    fprintf(stderr, "ClientBootstrap failed with error %s\n", ErrorDebugString(bootstrap.LastError()));
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
    fprintf(stderr,
          "Client Configuration initialization failed with error %s\n",
          ErrorDebugString(clientConfig.LastError()));
    return false;
  }

  Aws::Iot::MqttClient mqttClient(bootstrap);

  /*
   * Since no exceptions are used, always check the bool operator
   * when an error could have occurred.
   */
  if (!mqttClient)
  {
    fprintf(stderr, "MQTT Client Creation failed with error %s\n", ErrorDebugString(mqttClient.LastError()));
    return false;
  }
  auto connection = mqttClient.NewConnection(clientConfig);

  if (!*connection)
  {
      fprintf(stderr, "MQTT Connection Creation failed with error %s\n", ErrorDebugString(connection->LastError()));
    return false;
  }

  connection->OnConnectionCompleted = std::move(onConnectionCompleted);
  connection->OnDisconnect = std::move(onDisconnect);

  /*
   * Actually perform the connect dance.
   */
  fprintf(stdout, "Connecting...\n");
  if (!connection->Connect(clientId.c_str(), true, 0))
  {
    fprintf(stderr, "MQTT Connection failed with error %s\n", ErrorDebugString(connection->LastError()));
    return false;
  }

  if (connectionCompletedPromise.get_future().get())
  {
    IotIdentityClient identityClient(connection);

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
          fprintf(stderr, "Error publishing to CreateKeysAndCertificate: %s\n", ErrorDebugString(ioErr));
       }
       keysPublishCompletedPromise.set_value();
    };

    auto onKeysAcceptedSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
          fprintf(stderr, "Error subscribing to CreateKeysAndCertificate accepted: %s\n", ErrorDebugString(ioErr));
       }
       keysAcceptedCompletedPromise.set_value();
    };

    auto onKeysRejectedSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
          fprintf(stderr, "Error subscribing to CreateKeysAndCertificate rejected: %s\n", ErrorDebugString(ioErr));
       }
       keysRejectedCompletedPromise.set_value();
    };

    auto onKeysAccepted = [&](CreateKeysAndCertificateResponse *response, int ioErr)
    {
       if (ioErr == AWS_OP_SUCCESS)
       {
         fprintf(stdout, "CreateKeysAndCertificateResponse certificateId: %s.\n", response->CertificateId->c_str());
         token = *response->CertificateOwnershipToken;
         certificateContent = *response->CertificatePem;
         keyContent = *response->PrivateKey;
       }
       else
       {
          fprintf(stderr, "Error on subscription: %s.\n", ErrorDebugString(ioErr));
       }
    };

    auto onKeysRejected = [&](ErrorResponse *error, int ioErr)
    {
       if (ioErr == AWS_OP_SUCCESS)
       {
          fprintf(stdout,
               "CreateKeysAndCertificate failed with statusCode %d, errorMessage %s and errorCode %s.",
               *error->StatusCode,
               error->ErrorMessage->c_str(),
               error->ErrorCode->c_str());
       }
       else
       {
          fprintf(stderr, "Error on subscription: %s.\n", ErrorDebugString(ioErr));
       }
    };

    auto onRegisterAcceptedSubAck = [&](int ioErr) {
       if (ioErr != AWS_OP_SUCCESS)
       {
         fprintf(stderr, "Error subscribing to RegisterThing accepted: %s\n", ErrorDebugString(ioErr));
       }
       registerAcceptedCompletedPromise.set_value();
    };

    auto onRegisterRejectedSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
          fprintf(stderr, "Error subscribing to RegisterThing rejected: %s\n", ErrorDebugString(ioErr));
       }
       registerRejectedCompletedPromise.set_value();
    };

    auto onRegisterAccepted = [&](RegisterThingResponse *response, int ioErr)
    {
       if (ioErr == AWS_OP_SUCCESS)
       {
          fprintf(stdout, "RegisterThingResponse ThingName: %s.\n", response->ThingName->c_str());
       }
       else
       {
          fprintf(stderr, "Error on subscription: %s.\n", ErrorDebugString(ioErr));
       }
    };

    auto onRegisterRejected = [&](ErrorResponse *error, int ioErr)
    {
       if (ioErr == AWS_OP_SUCCESS)
       {
          fprintf(stdout,
               "RegisterThing failed with statusCode %d, errorMessage %s and errorCode %s.",
               *error->StatusCode,
               error->ErrorMessage->c_str(),
               error->ErrorCode->c_str());
       }
       else
       {
          fprintf(stderr, "Error on subscription: %s.\n", ErrorDebugString(ioErr));
       }
    };

    auto onRegisterPublishSubAck = [&](int ioErr)
    {
       if (ioErr != AWS_OP_SUCCESS)
       {
           fprintf(stderr, "Error publishing to RegisterThing: %s\n", ErrorDebugString(ioErr));
           exit(-1);
       }

       registerPublishCompletedPromise.set_value();
    };

    // CreateKeysAndCertificate workflow
    CreateKeysAndCertificateSubscriptionRequest keySubscriptionRequest;
    identityClient.SubscribeToCreateKeysAndCertificateAccepted(
        keySubscriptionRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onKeysAccepted, onKeysAcceptedSubAck);

    identityClient.SubscribeToCreateKeysAndCertificateRejected(
        keySubscriptionRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onKeysRejected, onKeysRejectedSubAck);

    CreateKeysAndCertificateRequest createKeysAndCertificateRequest;
    identityClient.PublishCreateKeysAndCertificate(
        createKeysAndCertificateRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onKeysPublishSubAck);

    RegisterThingSubscriptionRequest registerSubscriptionRequest;
    registerSubscriptionRequest.TemplateName = templateName;

    identityClient.SubscribeToRegisterThingAccepted(
        registerSubscriptionRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onRegisterAccepted, onRegisterAcceptedSubAck);

    identityClient.SubscribeToRegisterThingRejected(
        registerSubscriptionRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onRegisterRejected, onRegisterRejectedSubAck);

    Sleep(1000);

    RegisterThingRequest registerThingRequest;
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
  bool didAuthorise = false;
  if (!IsAuthorized())
    didAuthorise = DoAuthorize();
  else
    didAuthorise = true;

  if (didAuthorise)
  {
    Create();
  }
  else
  {
    CLog::Log(LOGDEBUG, "**MN** - CNWIoT::Listen() - Failed to 'DoAuthorize()' ");
  }
}

void CNWIoT::Process()
{
  SetPriority(THREAD_PRIORITY_NORMAL);
  #if ENABLE_NWIOT_DEBUGLOGS
  CLog::Log(LOGDEBUG, "**NW** - CNWIoT::Process Started");
  #endif
  ApiHandle apiHandle;
  apiHandle.InitializeLogging(Aws::Crt::LogLevel::Fatal, stderr);

  String topic = "TestTopic";
  String clientId(Aws::Crt::UUID().ToString());
  std::promise<bool> connectionCompletedPromise;
  std::promise<void> connectionClosedPromise;

      /*
   * This will execute when an mqtt connect has completed or failed.
   */
  auto onConnectionCompleted = [&](Mqtt::MqttConnection &, int errorCode, Mqtt::ReturnCode returnCode, bool) {
      if (errorCode)
      {
          fprintf(stdout, "Connection failed with error %s\n", ErrorDebugString(errorCode));
          connectionCompletedPromise.set_value(false);
      }
      else
      {
          if (returnCode != AWS_MQTT_CONNECT_ACCEPTED)
          {
              fprintf(stdout, "Connection failed with mqtt return code %d\n", (int)returnCode);
              connectionCompletedPromise.set_value(false);
          }
          else
          {
              fprintf(stdout, "Connection completed successfully.");
              connectionCompletedPromise.set_value(true);
          }
      }
  };

  auto onInterrupted = [&](Mqtt::MqttConnection &, int error) {
      fprintf(stdout, "Connection interrupted with error %s\n", ErrorDebugString(error));
  };

  auto onResumed = [&](Mqtt::MqttConnection &, Mqtt::ReturnCode, bool) { fprintf(stdout, "Connection resumed\n"); };

  /*
   * Invoked when a disconnect message has completed.
   */
  auto onDisconnect = [&](Mqtt::MqttConnection &) {
      {
          fprintf(stdout, "Disconnect completed\n");
          connectionClosedPromise.set_value();
      }
  };

  Io::EventLoopGroup eventLoopGroup(1);
  if (!eventLoopGroup)
  {
    fprintf(stderr, "Event Loop Group Creation failed with error %s\n", ErrorDebugString(eventLoopGroup.LastError()));
    return;
  }

  Aws::Crt::Io::DefaultHostResolver defaultHostResolver(eventLoopGroup, 1, 5);
  Io::ClientBootstrap bootstrap(eventLoopGroup, defaultHostResolver);

  if (!bootstrap)
  {
    fprintf(stderr, "ClientBootstrap failed with error %s\n", ErrorDebugString(bootstrap.LastError()));
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
    fprintf(stderr,
          "Client Configuration initialization failed with error %s\n",
          ErrorDebugString(clientConfig.LastError()));
    return;
  }

  Aws::Iot::MqttClient mqttClient(bootstrap);

  /*
   * Since no exceptions are used, always check the bool operator
   * when an error could have occurred.
   */
  if (!mqttClient)
  {
    fprintf(stderr, "MQTT Client Creation failed with error %s\n", ErrorDebugString(mqttClient.LastError()));
    return;
  }
  auto connection = mqttClient.NewConnection(clientConfig);

  if (!*connection)
  {
      CLog::Log(LOGDEBUG, "**MN** - CNWIoT::Process() - Failed to create client");
      return;
  }

  connection->OnConnectionCompleted = std::move(onConnectionCompleted);
  connection->OnDisconnect = std::move(onDisconnect);
  connection->OnConnectionInterrupted = std::move(onInterrupted);
  connection->OnConnectionResumed = std::move(onResumed);


  fprintf(stdout, "Connecting...\n");
  if (!connection->Connect(clientId.c_str(), false, 1000))
  {
      fprintf(stderr, "MQTT Connection failed with error %s\n", ErrorDebugString(connection->LastError()));
      exit(-1);
  }

  Sleep(1000);

  if (connectionCompletedPromise.get_future().get())
  {
    auto onPublish = [&](Mqtt::MqttConnection &, const String &topic, const ByteBuf &byteBuf)
    {
      CVariant resultObject;
      String payload((char *)byteBuf.buffer, byteBuf.len);
      CJSONVariantParser::Parse(payload.c_str(), resultObject);
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
                fprintf(stderr, "Subscribe failed with error %s\n", aws_error_debug_str(errorCode));
                exit(-1);
            }
            else
            {
                if (!packetId || QoS == AWS_MQTT_QOS_FAILURE)
                {
                    fprintf(stderr, "Subscribe rejected by the broker.");
                    exit(-1);
                }
                else
                {
                    fprintf(stdout, "Subscribe on topic %s on packetId %d Succeeded\n", topic.c_str(), packetId);
                }
            }
            subscribeFinishedPromise.set_value();
        };

      connection->Subscribe(topic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, onPublish, onSubAck);
      subscribeFinishedPromise.get_future().wait();
  }
  while (!m_bStop)
  {
    if (!m_payload.empty())
    {
      ByteBuf payload = ByteBufNewCopy(DefaultAllocator(), (const uint8_t *)m_payload.c_str(), m_payload.length());
      ByteBuf *payloadPtr = &payload;

      // set payload to "" so we dont go in here until
      // new msg has been set by announcer
      CNWIoT::setPayload("");
      auto onPublishComplete = [payloadPtr](Mqtt::MqttConnection &, uint16_t packetId, int errorCode)
      {
          aws_byte_buf_clean_up(payloadPtr);

          if (packetId)
          {
            fprintf(stdout, "Operation on packetId %d Succeeded\n", packetId);
          }
          else
          {
            fprintf(stdout, "Operation failed with error %s\n", aws_error_debug_str(errorCode));
          }
      };
      connection->Publish(topic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, false, payload, onPublishComplete);
    }
    std::string test;
  }
  std::promise<void> unsubscribeFinishedPromise;
  connection->Unsubscribe(
      topic.c_str(), [&](Mqtt::MqttConnection &, uint16_t, int) { unsubscribeFinishedPromise.set_value(); });
  //  unsubscribeFinishedPromise.get_future().wait();

  /* Disconnect */
  if (connection->Disconnect())
  {
      connectionClosedPromise.get_future().wait();
  }

}

void CNWIoT::setPayload(std::string payload)
{
  CSingleLock lock(m_payloadLock);
  m_payload = payload;
}
