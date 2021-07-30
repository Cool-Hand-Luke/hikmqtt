

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <queue>
#include <cjson/cJSON.h>

#include "HCNetSDK.h"
#include "main.h"
#include "mqtt.h"
#include "hik.h"

std::queue<char *> *messageQueue;
void CALLBACK MessageCallback(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser);
void ProcRuleAlarm(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser);

using namespace std;

hik_client::hik_client(std::queue <char *> *msgQ)
{
  messageQueue = msgQ;
  init_hik();
}

hik_client::~hik_client()
{
  // Stop listen server
  if ( iHandle >= 0 )
  {
    bool iRet = NET_DVR_StopListen_V30(iHandle);
    if (!iRet)
    {
      cout << "NET_DVR_StopListen_V30() Error: " << NET_DVR_GetLastError() << endl;
    }
  }

  // Log out all devices
  for (auto const& i : lDevices)
  {
    NET_DVR_Logout(i.userID);
  }

  // Release SDK resource
  NET_DVR_Cleanup();
}

void hik_client::init_hik()
{
  //---------------------------------------
  // Initialize
  NET_DVR_Init();

  //---------------------------------------
  // Set connection time and reconnection time
  NET_DVR_SetConnectTime(2000, 1);
  NET_DVR_SetReconnect(10000, true);

  //---------------------------------------
  // Our callback handler
  // ----
  // We only need one of these as the last one overwrites
  // any previous. So, move it here and save some CPU.
  NET_DVR_SetDVRMessageCallBack_V51(0, MessageCallback, NULL);

  //---------------------------------------
  // Compiled SDK Version
  SDK_Version();

  //---------------------------------------
  // Logging
  //NET_DVR_SetLogToFile(3, (char *)"./sdkLog");
}

void hik_client::SDK_Version()
{
  unsigned int uiVersion = NET_DVR_GetSDKBuildVersion();

  char strTemp[1024] = {0};
  sprintf(strTemp, "HCNetSDK V%d.%d.%d.%d\n", \
      (0xff000000 & uiVersion)>>24, \
      (0x00ff0000 & uiVersion)>>16, \
      (0x0000ff00 & uiVersion)>>8, \
      (0x000000ff & uiVersion));
  printf(strTemp);
}

void hik_client::ProcAlarmV30(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  NET_DVR_ALARMINFO_V30 struAlarmInfoV30;
  memcpy(&struAlarmInfoV30, pAlarmInfo, sizeof(NET_DVR_ALARMINFO_V30));
  printf("COMM_ALARM_V30: Alarm type is %d\n", struAlarmInfoV30.dwAlarmType);
}

void hik_client::ProcAlarmV40(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
   NET_DVR_ALARMINFO_V40 struAlarmInfoV40;
   memcpy(&struAlarmInfoV40, pAlarmInfo, sizeof(NET_DVR_ALARMINFO_V40));

   //cout << "dwAlarmType => " << struAlarmInfoV40.struAlarmFixedHeader.dwAlarmType << endl;

   switch (struAlarmInfoV40.struAlarmFixedHeader.dwAlarmType)
   {
     case 0x00: // Signal Alarm
       break;
     case 0x01: // HD Full Alarm
       break;
     case 0x02: // Loss of Signal
       break;
     case 0x03: // Motion Detection
       break;
     case 0x04: // HD Unformatted Alarm
       break;
     case 0x05: // HD Alarm
       break;
     case 0x06: // Block Alarm
       break;
     case 0x07: // System Alarm
       break;
     case 0x08: // Unauthorized Access
       break;
     case 0x09: // Video Exception
       break;
     case 0x0A: // Record Exception
       break;
     case 0x0B: // VCA Scene Change
       break;
     case 0x0C: // Array Exception
       break;
     case 0x0D: // Resolution Mismatch
       break;
     case 0x0F: // VCA Detection
       break;
     case 0x10: // Host Record Alarm
       break;
     case 0x28: // Minor Record Error?
       break;
     default:
       cout << "Unhandled Alarm (dwAlarmType == 0x" << hex << uppercase << struAlarmInfoV40.struAlarmFixedHeader.dwAlarmType << ")" << endl;
       break;
   }
}

// Time analysis macro definition
#define GET_YEAR(_time_)      (((_time_)>>26) + 2000)
#define GET_MONTH(_time_)     (((_time_)>>22) & 15)
#define GET_DAY(_time_)       (((_time_)>>17) & 31)
#define GET_HOUR(_time_)      (((_time_)>>12) & 31)
#define GET_MINUTE(_time_)    (((_time_)>>6)  & 63)
#define GET_SECOND(_time_)    (((_time_)>>0)  & 63)

void ProcRuleAlarm(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void *pUser)
{
  int devId = pAlarmer->lUserID;

  // New JSON object
  cJSON *hikEvent = cJSON_CreateObject();

  NET_VCA_RULE_ALARM struVcaRuleAlarm = { 0 };
  memcpy(&struVcaRuleAlarm, pAlarmInfo, sizeof(NET_VCA_RULE_ALARM));

  /*
  char chTime[128];
  NET_DVR_TIME struAbsTime = { 0 };
  struAbsTime.dwYear       = GET_YEAR(struVcaRuleAlarm.dwAbsTime);
  struAbsTime.dwMonth      = GET_MONTH(struVcaRuleAlarm.dwAbsTime);
  struAbsTime.dwDay        = GET_DAY(struVcaRuleAlarm.dwAbsTime);
  struAbsTime.dwHour       = GET_HOUR(struVcaRuleAlarm.dwAbsTime);
  struAbsTime.dwMinute     = GET_MINUTE(struVcaRuleAlarm.dwAbsTime);
  struAbsTime.dwSecond     = GET_SECOND(struVcaRuleAlarm.dwAbsTime);
  sprintf(chTime, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", struAbsTime.dwYear, struAbsTime.dwMonth, struAbsTime.dwDay, struAbsTime.dwHour, struAbsTime.dwMinute, struAbsTime.dwSecond);
  */

  /*
  const char *eventType;
  switch ((VCA_RULE_EVENT_TYPE_EX)struVcaRuleAlarm.struRuleInfo.wEventTypeEx)
  {
    case ENUM_VCA_EVENT_TRAVERSE_PLANE:
      eventType = "Line Crossing";
      break;
    case ENUM_VCA_EVENT_ENTER_AREA:
      eventType = "VCA_ENTER_AREA";
      break;
    case ENUM_VCA_EVENT_EXIT_AREA:
      eventType = "VCA_EXIT_AREA";
      break;
    case ENUM_VCA_EVENT_INTRUSION:
      eventType = "VCA_INTRUSION";
      break;
    case ENUM_VCA_EVENT_LOITER:
      eventType = "VCA_LOITER";
      break;
    case ENUM_VCA_EVENT_LEFT_TAKE:
      eventType = "VCA_LEFT_TAKE";
      break;
    case ENUM_VCA_EVENT_PARKING:
      eventType = "VCA_PARKING";
      break;
    case ENUM_VCA_EVENT_HIGH_DENSITY:
      eventType = "VCA_HIGH_DENSITY";
      break;
    default:
      eventType = "Unknown";
      break;
  }
  cJSON_AddStringToObject(hikEvent, "Alarm", eventType);
  */

  cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
  //cJSON_AddStringToObject(hikEvent, "IP", (char *) struVcaRuleAlarm.struDevInfo.struDevIP.sIpV4);
  cJSON_AddNumberToObject(hikEvent, "EventType", (int) struVcaRuleAlarm.struRuleInfo.wEventTypeEx);
  cJSON_AddNumberToObject(hikEvent, "Channel", (int) struVcaRuleAlarm.struDevInfo.byChannel);
  cJSON_AddNumberToObject(hikEvent, "IvmsChannel", (int) struVcaRuleAlarm.struDevInfo.byIvmsChannel);
  cJSON_AddNumberToObject(hikEvent, "RuleID", (int) struVcaRuleAlarm.struRuleInfo.byRuleID);
  cJSON_AddNumberToObject(hikEvent, "dwID", (int) struVcaRuleAlarm.struTargetInfo.dwID);
  messageQueue->push(cJSON_Print(hikEvent));
  cJSON_Delete(hikEvent);

  /*
  cout << "-----------" << endl;
  cout << (char *) struVcaRuleAlarm.struRuleInfo.byRuleName << endl;
  cout << chTime << ":";
  cout << (char *) struVcaRuleAlarm.struDevInfo.struDevIP.sIpV4 << ", ";
  cout << (int) struVcaRuleAlarm.struDevInfo.byChannel << ", ";
  cout << (int) struVcaRuleAlarm.struDevInfo.byIvmsChannel << ", ";
  cout << (int) struVcaRuleAlarm.struRuleInfo.byRuleID << ", ";
  cout << struVcaRuleAlarm.dwAlarmID << ", ";
  cout << struVcaRuleAlarm.struRuleInfo.dwEventType << ", ";
  cout << struVcaRuleAlarm.struRuleInfo.wEventTypeEx << ", ";

  // NET_VCA_TARGET_INFO
  cout << struVcaRuleAlarm.struTargetInfo.dwID << ", ";

  cout << "Rect: " << struVcaRuleAlarm.struTargetInfo.struRect.fX << ", " << struVcaRuleAlarm.struTargetInfo.struRect.fY << ", ";
  cout << struVcaRuleAlarm.struTargetInfo.struRect.fWidth << ", " << struVcaRuleAlarm.struTargetInfo.struRect.fHeight << ", ";
  cout << "\n";
  cout << struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struStart.fX << ", ";
  cout << struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struStart.fY << ", ";
  cout << struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struEnd.fX << ", ";
  cout << struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struEnd.fY << ", ";
  cout << struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.dwCrossDirection << ", ";

  cout << eventType << endl;
  */
}

void CALLBACK MessageCallback(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser)
{
  //cout << "MessageCallback: ";
  //cout << hex << uppercase << lCommand << endl;

  switch (lCommand)
  {
    case COMM_IPC_AUXALARM_RESULT:
      break;
    case COMM_ALARM:
      break;
    case COMM_ALARM_V30:
      //ProcAlarmV30(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_ALARM_V40:
      //ProcAlarmV40(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_ALARM_RULE:
      ProcRuleAlarm(lCommand, pAlarmer, pAlarmInfo, dwBufLen, pUser);
      break;
    case COMM_ALARM_PDC:
      break;
    case COMM_SENSOR_ALARM:
      break;
    case COMM_ALARM_FACE:
      break;
    default:
      cout << "Unhandled <MessageCallback>: " << hex << uppercase << lCommand << endl;
      break;
  }
}

int hik_client::listen_server(const char *ipAddr, const unsigned int port)
{
  iHandle = NET_DVR_StartListen_V30((char *)ipAddr, port, MessageCallback, NULL);

  if ( iHandle < 0 )
  {
    cout << "NET_DVR_StartListen_V30() Error: " << NET_DVR_GetLastError() << endl;
  }

  return iHandle;
}

int hik_client::add_source(const char *devId, const char *ipAddr, const char *username, const char *password)
{
  //---------------------------------------
  // Prepare to login to device
  NET_DVR_USER_LOGIN_INFO struLoginInfo = {0};
  NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = {0};
  struLoginInfo.bUseAsynLogin = false;

  struLoginInfo.wPort = 8000;
  strncpy(struLoginInfo.sDeviceAddress, ipAddr, NET_DVR_DEV_ADDRESS_MAX_LEN);
  strncpy(struLoginInfo.sUserName, username, NAME_LEN);
  strncpy(struLoginInfo.sPassword, password, NAME_LEN);

  //---------------------------------------
  // Log in to device
  int id = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);
  if (id < 0)
  {
     printf("Login error, %d\n", NET_DVR_GetLastError());
     return false;
  }

  //---------------------------------------
  // Enable Arming Mode V5.1
  NET_DVR_SETUPALARM_PARAM_V50 struSetupAlarmParam = { 0 };
  struSetupAlarmParam.dwSize = sizeof(struSetupAlarmParam);
  struSetupAlarmParam.byRetVQDAlarmType = TRUE;
  struSetupAlarmParam.byRetAlarmTypeV40 = TRUE;
  struSetupAlarmParam.byFaceAlarmDetection = 0;
  struSetupAlarmParam.byRetDevInfoVersion = TRUE;
  struSetupAlarmParam.byAlarmInfoType = 1;
  struSetupAlarmParam.bySupport = 4;
  long handle = NET_DVR_SetupAlarmChan_V50(id, &struSetupAlarmParam, NULL, 0);

  if (handle < 0)
  {
    printf("NET_DVR_SetupAlarmChan_V50 error, %d\n", NET_DVR_GetLastError());
    NET_DVR_Logout(id);
    return false;
  }

  _dev_info_ camera = {0};
  camera.devId = strdup(devId);
  camera.userID = id;
  lDevices.push_back(camera);

  return id;
}

