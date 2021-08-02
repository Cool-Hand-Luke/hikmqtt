

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <queue>
#include <math.h>
#include <cjson/cJSON.h>

#include "HCNetSDK.h"
#include "main.h"
#include "mqtt.h"
#include "hik.h"

std::queue<char *> *messageQueue;
void CALLBACK MessageCallback(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser);

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
      printf("NET_DVR_StopListen_V30() Error: %d\n", NET_DVR_GetLastError());
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

void ProcAlarmV30(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  NET_DVR_ALARMINFO_V30 struAlarmInfoV30;
  memcpy(&struAlarmInfoV30, pAlarmInfo, sizeof(NET_DVR_ALARMINFO_V30));
  printf("COMM_ALARM_V30: Alarm type is %d\n", struAlarmInfoV30.dwAlarmType);

  switch(struAlarmInfoV30.dwAlarmType)
  {
    case 0x00: // Signal Alarm
      break;
    case 0x01: // HD Full Alarm
      break;
    case 0x02: // Loss of Signal
      break;
    case 0x03: // Motion Detection
      // Motion detection is pretty useless for further analysis.
      /*
      for (int i=0; i<16; i++)
      {
        if (struAlarmInfoV30.byChannel[i] == 1)
        {
          printf("Motion detection %d\n", i+1);
        }
      }
      */
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
      printf("Unhandled Alarm (dwAlarmType == 0x%0X)\n", struAlarmInfoV30.dwAlarmType);
      break;
  }
}

void ProcAlarmV40(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  NET_DVR_ALARMINFO_V40 struAlarmInfoV40;
  memcpy(&struAlarmInfoV40, pAlarmInfo, sizeof(NET_DVR_ALARMINFO_V40));

  printf("COMM_ALARM: Alarm type is %d\n", struAlarmInfoV40.struAlarmFixedHeader.dwAlarmType);

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
      printf("Unhandled Alarm (dwAlarmType == 0x%0X)\n", struAlarmInfoV40.struAlarmFixedHeader.dwAlarmType);
      break;
  }
}

void ProcDevStatusChanged(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
}

void procGISInfoAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  NET_DVR_GIS_UPLOADINFO  struGISInfo = { 0 };
  memcpy(&struGISInfo, pAlarmInfo, sizeof(struGISInfo));

  /*
  printf("--------------------------\n");
  printf("PtzPos{PanPos: %.3f, TiltPos: %.1f, ZoomPos: %.1f}\n",struGISInfo.struPtzPos.fPanPos, struGISInfo.struPtzPos.fTiltPos, struGISInfo.struPtzPos.fZoomPos);
  printf("RelTime: %d, AbsTime: %d, Azimuth: %f, LongitudeType: %d, LatitudeType: %d\n", struGISInfo.dwRelativeTime, struGISInfo.dwAbsTime, struGISInfo.fAzimuth, struGISInfo.byLongitudeType, struGISInfo.byLatitudeType);
  printf("Longitude{Sec: %.6f, fDegree: %d, Minute: %d}\n", struGISInfo.struLongitude.fSec, struGISInfo.struLongitude.byDegree, struGISInfo.struLongitude.byMinute);
  printf("Latitude{Sec: %.6f, Degree: %d, Minute: %d}\n", struGISInfo.struLatitude.fSec, struGISInfo.struLatitude.byDegree, struGISInfo.struLatitude.byMinute);
  printf("Horizontal: %f, Vertical: %f, VisibleRadius: %f, MaxView: %f\n", struGISInfo.fHorizontalValue, struGISInfo.fVerticalValue, struGISInfo.fVisibleRadius, struGISInfo.fMaxViewRadius);
  printf("Sensor{Type: %d, fHorWidth: %f, fVerWidth: %f, Fold: %f}\n", struGISInfo.struSensorParam.bySensorType, struGISInfo.struSensorParam.fHorWidth, struGISInfo.struSensorParam.fVerWidth, struGISInfo.struSensorParam.fFold);
  printf("DevInfo{DevIP: %s, Port: %d, Chan: %d, IvmsChan: %d}\n", struGISInfo.struDevInfo.struDevIP.sIpV4, struGISInfo.struDevInfo.wPort, struGISInfo.struDevInfo.byChannel, struGISInfo.struDevInfo.byIvmsChannel);
  */
}

// Time analysis macro definition
#define GET_YEAR(_time_)      (((_time_)>>26) + 2000)
#define GET_MONTH(_time_)     (((_time_)>>22) & 15)
#define GET_DAY(_time_)       (((_time_)>>17) & 31)
#define GET_HOUR(_time_)      (((_time_)>>12) & 31)
#define GET_MINUTE(_time_)    (((_time_)>>6)  & 63)
#define GET_SECOND(_time_)    (((_time_)>>0)  & 63)

void ProcRuleAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  int devId = pAlarmer->lUserID;

  // New JSON object
  cJSON *hikEvent = cJSON_CreateObject();

  NET_VCA_RULE_ALARM struVcaRuleAlarm = { 0 };
  memcpy(&struVcaRuleAlarm, pAlarmInfo, sizeof(NET_VCA_RULE_ALARM));

  //cJSON_AddStringToObject(hikEvent, "Type", "Alarm");
  cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
  cJSON_AddNumberToObject(hikEvent, "ts", (long) struVcaRuleAlarm.dwAbsTime);
  //cJSON_AddStringToObject(hikEvent, "IP", (char *) struVcaRuleAlarm.struDevInfo.struDevIP.sIpV4);
  cJSON_AddNumberToObject(hikEvent, "EventType", (int) struVcaRuleAlarm.struRuleInfo.wEventTypeEx);
  cJSON_AddNumberToObject(hikEvent, "Channel", (int) struVcaRuleAlarm.struDevInfo.byChannel);
  cJSON_AddNumberToObject(hikEvent, "IvmsChannel", (int) struVcaRuleAlarm.struDevInfo.byIvmsChannel);
  cJSON_AddNumberToObject(hikEvent, "RuleID", (int) struVcaRuleAlarm.struRuleInfo.byRuleID);
  cJSON_AddNumberToObject(hikEvent, "dwID", (int) struVcaRuleAlarm.struTargetInfo.dwID);
  messageQueue->push(cJSON_Print(hikEvent));
  cJSON_Delete(hikEvent);

  /*
  printf("-----------\n");
  printf("struRuleInfo.byRuleName: %s\n", (char *) struVcaRuleAlarm.struRuleInfo.byRuleName);
  printf("struDevInfo.struDevIP.sIpV4: %s\n", (char *) struVcaRuleAlarm.struDevInfo.struDevIP.sIpV4);
  printf("struDevInfo.byChannel: %d\n", (int) struVcaRuleAlarm.struDevInfo.byChannel);
  printf("struDevInfo.byIvmsChannel: %d\n", (int) struVcaRuleAlarm.struDevInfo.byIvmsChannel);
  printf("struRuleInfo.byRuleID: %d\n", (int) struVcaRuleAlarm.struRuleInfo.byRuleID);
  printf("struVcaRuleAlarm.dwAlarmID: %d\n", struVcaRuleAlarm.dwAlarmID);
  printf("struRuleInfo.dwEventType: %d\n", struVcaRuleAlarm.struRuleInfo.dwEventType);
  printf("struRuleInfo.wEventTypeEx: %d\n", struVcaRuleAlarm.struRuleInfo.wEventTypeEx);

  // Information not provided by all DVR/NVR devices
  // NET_VCA_TARGET_INFO
  printf("struTargetInfo.dwID: %d\n", struVcaRuleAlarm.struTargetInfo.dwID);

  printf("Rect: struRect.fX: %f, struRect.fY: %f, fWidth: %f, fHeight: %f\n",
      struVcaRuleAlarm.struTargetInfo.struRect.fX, struVcaRuleAlarm.struTargetInfo.struRect.fY,
      struVcaRuleAlarm.struTargetInfo.struRect.fWidth, struVcaRuleAlarm.struTargetInfo.struRect.fHeight);

  printf("Dir: struStart.fX: %f, struStart.fY: %f -> struEnd.fX: %f, struEnd.fY: %f (dwCrossDirection: %d)\n",
      struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struStart.fX,
      struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struStart.fY,
      struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struEnd.fX,
      struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struEnd.fY,
      struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.dwCrossDirection);

  float x = (struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struEnd.fX - struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struStart.fX);
  float y = (struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struEnd.fY - struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struStart.fY);
  printf("Distance: %f\n", sqrt((x*x)+(y*y)));
  */
}

void CALLBACK MessageCallback(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser)
{
  //printf("MessageCallback: 0x%0X\n", lCommand);

  switch (lCommand)
  {
    case COMM_IPC_AUXALARM_RESULT:
      printf("Implement: COMM_IPC_AUXALARM_RESULT\n");
      break;
    case COMM_ALARM:
      printf("COMM_ALARM\n");
      break;
    case COMM_ALARM_V30:
      printf("COMM_ALARM_V30\n");
      break;
    case COMM_ALARM_V40:
      printf("COMM_ALARM_V40\n");
      ProcAlarmV40(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_ALARM_RULE:
      printf("COMM_ALARM_RULE\n");
      ProcRuleAlarm(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_ALARM_PDC:
      printf("Implement: COMM_ALARM_PDC\n");
      break;
    case COMM_SENSOR_ALARM:
      printf("Implement: COMM_SENSOR_ALARM\n");
      break;
    case COMM_ALARM_FACE:
      printf("Implement: COMM_ALARM_FACE\n");
      break;
    case COMM_ALARM_AUDIOEXCEPTION:
      printf("Implement: COMM_ALARM_AUDIOEXCEPTION\n");
      break;
    case COMM_GISINFO_UPLOAD:
      printf("COMM_GISINFO_UPLOAD\n");
      procGISInfoAlarm(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_DEV_STATUS_CHANGED:
      printf("COMM_DEV_STATUS_CHANGED\n");
      ProcDevStatusChanged(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    default:
      printf("Unhandled <MessageCallback>: 0x%0X\n", lCommand);
      break;
  }
}

int hik_client::listen_server(string ipAddr, const unsigned int port)
{
  iHandle = NET_DVR_StartListen_V30((char *)ipAddr.c_str(), port, MessageCallback, NULL);

  if ( iHandle < 0 )
  {
    printf("NET_DVR_StartListen_V30() Error: %d\n", NET_DVR_GetLastError());
  }

  return iHandle;
}

int hik_client::add_source(string devId, string ipAddr, string username, string password)
{
  //---------------------------------------
  // Prepare to login to device
  NET_DVR_USER_LOGIN_INFO struLoginInfo = {0};
  NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = {0};
  struLoginInfo.bUseAsynLogin = false;

  struLoginInfo.wPort = 8000;
  strncpy(struLoginInfo.sDeviceAddress, ipAddr.c_str(), NET_DVR_DEV_ADDRESS_MAX_LEN);
  strncpy(struLoginInfo.sUserName, username.c_str(), NAME_LEN);
  strncpy(struLoginInfo.sPassword, password.c_str(), NAME_LEN);

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
  struSetupAlarmParam.byLevel = 1;                  // Arming priority: 0-high, 1-medium, 2-low
  struSetupAlarmParam.byAlarmInfoType = 1;          // Intel. traffic alarm type: 0-old (NET_DVR_PLATE_RESULT),1-new (NET_ITS_PLATE_RESULT).
  struSetupAlarmParam.byRetAlarmTypeV40 = 1;        // Motion detection, video loss, video tampering, and alarm input alarm information is sent NET_DVR_ALARMINFO_V40
  struSetupAlarmParam.byRetDevInfoVersion = 1;      // Alarm types of CVR: 0 = NET_DVR_ALARMINFO_DEV, 1 = NET_DVR_ALARMINFO_DEV_V40
  struSetupAlarmParam.byRetVQDAlarmType = 1;        // VQD alarm types: 0 = COMM_ALARM_VQD, 1 = COMM_ALARM_VQD_EX
  struSetupAlarmParam.byFaceAlarmDetection = 1;     // 0 = COMM_UPLOAD_FACESNAP_RESULT, 1 = NET_DVR_FACE_DETECTION
  struSetupAlarmParam.bySupport = 0x0c;             // bit0: Whether to upload picture: 0 = yes, 1 = no
                                                    // bit1: Whether to enable ANR: 0 = no, 1 = yes
                                                    // bit4: Whether to upload behavior analysis events of all detection targets: 0 = no, 1 = yes
                                                    // bit5: Whether to enable all-day event or alarm uploading: 0 = no, 1 = yes
  //struSetupAlarmParam.byBrokenNetHttp = 0;          //
  //struSetupAlarmParam.byDeployType = 0;             // Arming type: 0 = arm via client software, 1 = real-time arming.
  //struSetupAlarmParam.bySubScription = 0;           // Bit7: Whether to upload picture after subscribing motion detection: 0 = no, 1 = yes

  long handle = NET_DVR_SetupAlarmChan_V50(id, &struSetupAlarmParam, NULL, 0);

  if (handle < 0)
  {
    printf("NET_DVR_SetupAlarmChan_V50 error, %d\n", NET_DVR_GetLastError());
    NET_DVR_Logout(id);
    return false;
  }

  /* Just playing, here
  DWORD dwReturn = 0;
  UINT  m_iFollowChan;
  UINT  m_iMainChan;
  if (NET_DVR_GetDVRConfig(handle, NET_DVR_GET_PTZPOS, m_iFollowChan, &m_struCurCBPPoint.struPtzPos, sizeof(NET_DVR_PTZPOS), &dwReturn))
  {
    printf("NET_DVR_GET_PTZPOS %d t%d z%d\n", m_struCurCBPPoint.struPtzPos.wPanPos, m_struCurCBPPoint.struPtzPos.wTiltPos, m_struCurCBPPoint.struPtzPos.wZoomPos);
    m_struCurCBPPoint.struPtzPos.wPanPos += 100;
    NET_DVR_SetDVRConfig(handle, NET_DVR_SET_PTZPOS, m_iFollowChan, &m_struCurCBPPoint.struPtzPos, sizeof(NET_DVR_PTZPOS));
    NET_DVR_PTZPreset_Other(handle, 1, GOTO_PRESET, 4);
    //NET_DVR_PTZPreset(handle, GOTO_PRESET, 1);
    //NET_DVR_PTZPreset_EX(handle, GOTO_PRESET, 1);
  }
  */

  _dev_info_ camera = {0};
  camera.devId = strdup(devId.c_str());
  camera.userID = id;
  lDevices.push_back(camera);

  return id;
}

