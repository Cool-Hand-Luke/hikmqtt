

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <queue>
#include <algorithm>
#include <cjson/cJSON.h>

#include "HCNetSDK.h"
#include "main.h"
#include "mqtt.h"
#include "hik.h"

//#define HIK_DEBUG
#define SDK_LOGLEVEL 0

using namespace std;
std::queue<char *> *messageQueue;

void CALLBACK MessageCallback_V51(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser);
BOOL CALLBACK MessageCallback_V31(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser);
void CALLBACK ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser);

/***************************************************************************/
/*  Debug helper.                                                          */
/***************************************************************************/
// Source: https://codereview.stackexchange.com/questions/165120/printing-hex-dumps-for-diagnostics
// -----------------
std::ostream& hex_dump(std::ostream& os, const void *buffer, std::size_t bufsize, bool showPrintableChars = true)
{
  if (buffer == nullptr)
  {
    return os;
  }
  auto oldFormat = os.flags();
  auto oldFillChar = os.fill();
  constexpr std::size_t maxline{20};
  // create a place to store text version of string
  char renderString[maxline+1];
  char *rsptr{renderString};
  // convenience cast
  const unsigned char *buf{reinterpret_cast<const unsigned char *>(buffer)};

  for (std::size_t linecount=maxline; bufsize; --bufsize, ++buf)
  {
    os << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(*buf) << ' ';
    *rsptr++ = std::isprint(*buf) ? *buf : '.';
    if (--linecount == 0)
    {
      *rsptr++ = '\0';  // terminate string
      if (showPrintableChars)
      {
        os << " | " << renderString;
      }
      os << '\n';
      rsptr = renderString;
      linecount = std::min(maxline, bufsize);
    }
  }
  // emit newline if we haven't already
  if (rsptr != renderString)
  {
    if (showPrintableChars)
    {
      for (*rsptr++ = '\0'; rsptr != &renderString[maxline+1]; ++rsptr)
      {
        os << "   ";
      }
      os << " | " << renderString;
    }
    os << '\n';
  }

  os.fill(oldFillChar);
  os.flags(oldFormat);
  return os;
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void CALLBACK ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
{
  std::cerr << "ExceptionCallBack lUserID: " << lUserID << ", handle: " << lHandle << ", user data: %p" << pUser << std::endl;

  switch(dwType)
  {
    case EXCEPTION_AUDIOEXCHANGE:
      std::cerr << "Audio exchange exception!" << std::endl;
      break;
    case EXCEPTION_ALARM:
      std::cerr << "Alarm exception!" << std::endl;
      break;
    case EXCEPTION_ALARMRECONNECT:
      std::cerr << "Alarm reconnect." << std::endl;
      break;
    case ALARM_RECONNECTSUCCESS:
      std::cerr << "Alarm reconnect success." << std::endl;
      break;
    case EXCEPTION_SERIAL:
      std::cerr << "Serial exception!" << std::endl;
      break;
    case EXCEPTION_PREVIEW:
      std::cerr << "Preview exception!" << std::endl;
      break;
    case EXCEPTION_RECONNECT:
      std::cerr << "preview reconnecting." << std::endl;
      break;
  case PREVIEW_RECONNECTSUCCESS:
      std::cerr << "Preview reconncet success." << std::endl;
      break;
  default:
    break;
  }
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void CALLBACK MessageCallback_V30(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void *pUser)
{
  hik_client *ptr = (hik_client *)pUser;
  ptr->proc_callback_message(lCommand, pAlarmer, pAlarmInfo, dwBufLen);
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
BOOL CALLBACK MessageCallback_V31(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void *pUser)
{
  hik_client *ptr = (hik_client *)pUser;
  ptr->proc_callback_message(lCommand, pAlarmer, pAlarmInfo, dwBufLen);

  return true;
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void CALLBACK MessageCallback_V51(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void *pUser)
{
  hik_client *ptr = (hik_client *)pUser;
  ptr->proc_callback_message(lCommand, pAlarmer, pAlarmInfo, dwBufLen);
}

hik_client::hik_client(std::queue <char *> *msgQ)
{
  messageQueue = msgQ;
  init_hik();
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
_dev_info_ *hik_client::get_device_byDevId(int devId)
{
  _dev_info_ *dev = NULL;

  // Iterate through our cameras to start receiving notifications
  std::list <_dev_info_>::iterator it;
  for (it = lDevices.begin(); it != lDevices.end(); it++)
  {
    if ( it->devId == devId )
    {
      dev = &(*it);
      break;
    }
  }

  return dev;
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
_dev_info_ *hik_client::get_device_byUserId(DWORD userId)
{
  _dev_info_ *dev = NULL;

  // Iterate through our cameras to start receiving notifications
  std::list <_dev_info_>::iterator it;
  for (it = lDevices.begin(); it != lDevices.end(); it++)
  {
    if ( it->userId == userId )
    {
      dev = &(*it);
      break;
    }
  }

  return dev;
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
_dev_info_ *hik_client::get_device_byHandle(DWORD handle)
{
  _dev_info_ *dev = NULL;

  // Iterate through our cameras to start receiving notifications
  std::list <_dev_info_>::iterator it;
  for (it = lDevices.begin(); it != lDevices.end(); it++)
  {
    if ( it->handle == handle )
    {
      dev = &(*it);
      break;
    }
  }

  return dev;
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
hik_client::~hik_client()
{
  // Stop listen server
  if ( iHandle >= 0 )
  {
    bool iRet = NET_DVR_StopListen_V30(iHandle);
    if (!iRet)
    {
      int lError = NET_DVR_GetLastError();
      printf("NET_DVR_StopListen_V30() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
    }
  }

  // Log out all devices
  for (auto const& i : lDevices)
  {
    NET_DVR_Logout(i.userId);
  }

  // Release SDK resource
  NET_DVR_Cleanup();
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void hik_client::init_hik()
{
  //---------------------------------------
  // Initialize
  NET_DVR_Init();

  //---------------------------------------
  // Set connection time and reconnection time
  NET_DVR_SetConnectTime(2000, 3);
  NET_DVR_SetReconnect(1000, true);

  //---------------------------------------
  // Our exception handler
  // ----
  NET_DVR_SetExceptionCallBack_V30(0, nullptr, ExceptionCallBack, this);

  //---------------------------------------
  // Our callback handler
  // ----
  NET_DVR_SetDVRMessageCallBack_V31(MessageCallback_V31, this);
  //NET_DVR_SetDVRMessageCallBack_V51(0, MessageCallback_V51, this);

  //---------------------------------------
  // Compiled SDK Version
  SDK_Version();

  //---------------------------------------
  // Logging, 1 = Err, 2 = Err + Dev, 3 = All
  NET_DVR_SetLogToFile(SDK_LOGLEVEL, NULL);
}

/***************************************************************************/
/* Display the SDK version we were compiled with.                          */
/***************************************************************************/
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

/***************************************************************************/
/* Get and return the current camera pan, tilt, zoom position.             */
/***************************************************************************/
void hik_client::get_ptz_pos(int devId, long channel)
{
  int cur_preset = -1;

  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    DWORD dwReturn = 0;
    NET_DVR_PTZPOS struPtzPos = {0};

    if (!NET_DVR_GetDVRConfig(dev->userId, NET_DVR_GET_PTZPOS, channel, &struPtzPos, sizeof(struPtzPos), &dwReturn))
    {
      int lError = NET_DVR_GetLastError();
      printf("get_ptz_pos() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
    }
    else
    {
    }
  }

  //return cur_preset;
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void hik_client::get_dvr_config(int devId, long channel)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    DWORD dwReturn = 0;
    NET_DVR_DEVICECFG_V40 struDevCfg = {0};

    if (!NET_DVR_GetDVRConfig(dev->userId, NET_DVR_GET_DEVICECFG_V40, 0, &struDevCfg, sizeof(NET_DVR_DEVICECFG_V40), &dwReturn))
    {
      int lError = NET_DVR_GetLastError();
      printf("get_preset_names() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
    }
    else
    {
      std::cout << "--------------------" << std::endl;
      std::cout << "devId: " << devId << std::endl;
      std::cout << "DVR Name: " << struDevCfg.sDVRName << std::endl;
      std::cout << "DVR ID: " << struDevCfg.dwDVRID << std::endl;
      std::cout << "Serial Number: " << struDevCfg.sSerialNumber << std::endl;
      std::cout << "Dev Type Name: " << struDevCfg.byDevTypeName << std::endl;
      std::cout << "VGA Ports: " << struDevCfg.byVGANum << std::endl;
    }
  }
}
void hik_client::set_dvr_config(int devId, long channel)
{
  //NET_DVR_SetDVRConfig(g_pMainDlg->m_struDeviceInfo.lLoginID, NET_DVR_SET_PTZPOS, 0, &m_ptzPos, sizeof(NET_DVR_PTZPOS));
}

void hik_client::get_preset_details(int devId, long channel, int presetIndx)
{
}

/***************************************************************************/
/*  Get a list of all the presets for the specified device                 */
/***************************************************************************/
int hik_client::update_preset_names(int devId, long channel)
{
  int cur_preset = -1;

  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    DWORD dwReturn = 0;
    NET_DVR_PTZPOS struPtzPos = {0};

    if (!NET_DVR_GetDVRConfig(dev->userId, NET_DVR_GET_PRESET_NAME, dev->struDeviceInfoV40.struDeviceV30.byStartChan, &dev->struParams, sizeof(NET_DVR_PRESET_NAME) * MAX_PRESET_V40, &dwReturn))
    {
      int lError = NET_DVR_GetLastError();
      printf("get_preset_names() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
    }
    else
    {
      cur_preset = 0;
    }
  }

  return cur_preset;
}

/***************************************************************************/
/*  Set / Call / Clear the specified preset.                               */
/***************************************************************************/
void hik_client::ptz_preset(int devId, long channel, int ptzCmd, int presetIndx)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    NET_DVR_PTZPreset_Other(dev->userId, channel, ptzCmd, presetIndx);
  }
}

/***************************************************************************/
/*  Zoom / Focus / Tilt / Pan the specified device.                        */
/***************************************************************************/
void hik_client::ptz_controlwithspeed(int devId, long channel, int dir, int speed)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    // Control with speed start
    if (!NET_DVR_PTZControlWithSpeed_Other(dev->userId, channel, dir, 0, speed))
    {
      int lError = NET_DVR_GetLastError();
      printf("get_cur_preset() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
    }
    else
    {
      std::cout << "Set" << std::endl;
      usleep(1000000);
      std::cout << "end" << std::endl;
      // Control with speed end
      NET_DVR_PTZControlWithSpeed_Other(dev->userId, channel, dir, 1, speed);
    }
  }
}

/***************************************************************************/
/*  COMM_ALARM_V30 callback.                                               */
/***************************************************************************/
void hik_client::ProcAlarmV30(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
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

/***************************************************************************/
/*  COMM_ALARM_V40 callback.                                               */
/***************************************************************************/
void hik_client::ProcAlarmV40(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  NET_DVR_ALARMINFO_V40 struAlarmInfoV40;
  memcpy(&struAlarmInfoV40, pAlarmInfo, sizeof(NET_DVR_ALARMINFO_V40));

  printf("COMM_ALARM_V40: Alarm type is %d\n", struAlarmInfoV40.struAlarmFixedHeader.dwAlarmType);

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

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void hik_client::ProcDevStatusChanged(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  int devId = get_device_byUserId(pAlarmer->lUserID)->devId;

  std::cout << "Device: " << devId << std::endl;
  //hex_dump(std::cout, pAlarmInfo, dwBufLen);
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void hik_client::procGISInfoAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  int devId = get_device_byUserId(pAlarmer->lUserID)->devId;

  NET_DVR_GIS_UPLOADINFO  struGISInfo = { 0 };
  memcpy(&struGISInfo, pAlarmInfo, sizeof(struGISInfo));

  //hex_dump(std::cout, pAlarmInfo, dwBufLen);
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

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void hik_client::ProcRuleAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  int devId = get_device_byUserId(pAlarmer->lUserID)->devId;

  // New JSON object
  cJSON *hikEvent = cJSON_CreateObject();

  NET_VCA_RULE_ALARM struVcaRuleAlarm = { 0 };
  memcpy(&struVcaRuleAlarm, pAlarmInfo, sizeof(NET_VCA_RULE_ALARM));

  //cJSON_AddStringToObject(hikEvent, "Type", "Alarm");
  cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
  //cJSON_AddNumberToObject(hikEvent, "uid", (int) pAlarmer->lUserID);
  //cJSON_AddNumberToObject(hikEvent, "ts", (long) struVcaRuleAlarm.dwAbsTime);
  //cJSON_AddStringToObject(hikEvent, "IP", (char *) struVcaRuleAlarm.struDevInfo.struDevIP.sIpV4);
  cJSON_AddNumberToObject(hikEvent, "EventType", (int) struVcaRuleAlarm.struRuleInfo.wEventTypeEx);
  cJSON_AddNumberToObject(hikEvent, "Channel", (int) struVcaRuleAlarm.struDevInfo.byChannel);
  cJSON_AddNumberToObject(hikEvent, "IvmsChannel", (int) struVcaRuleAlarm.struDevInfo.byIvmsChannel);
  cJSON_AddNumberToObject(hikEvent, "RuleID", (int) struVcaRuleAlarm.struRuleInfo.byRuleID);
  if ( struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.dwCrossDirection )
  {
    cJSON_AddNumberToObject(hikEvent, "dwDir", (int) struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.dwCrossDirection);
  }
  //cJSON_AddNumberToObject(hikEvent, "dwID", (int) struVcaRuleAlarm.struTargetInfo.dwID);
  messageQueue->push(cJSON_Print(hikEvent));
  cJSON_Delete(hikEvent);

  /* Using these values to calculate direction seems impossible.
   * Having delved into it, the devices, regardless of direction, return a line calculated from the left
   * hand side of the screen to the right. So best we can do, is calculate an angle of travel which could be
   * travelling in 1 of 2 directions.
   */
  /*
  float x = (struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struEnd.fX - struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struStart.fX);
  float y = (struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struEnd.fY - struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.struPlaneBottom.struStart.fY);
  printf("Magnitude: %f\n", sqrt((x*x)+(y*y)));
  printf("Vector = %f, %f\n",  x, y);
  printf("Angle = %f\n", (atan(y/x) * 180 / 3.142));
  */

#ifdef HIK_DEBUG
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
#endif
}

/***************************************************************************/
/* Where we process the data received via callbacks.                       */
/***************************************************************************/
void hik_client::proc_callback_message(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  switch (lCommand)
  {
    case COMM_IPC_AUXALARM_RESULT:
      printf("Implement: COMM_IPC_AUXALARM_RESULT\n");
      break;
    case COMM_ALARM:
      printf("COMM_ALARM\n");
      break;
    case COMM_ALARM_V30:
      //printf("COMM_ALARM_V30\n");
      ProcAlarmV30(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_ALARM_V40:
      //printf("COMM_ALARM_V40\n");
      ProcAlarmV40(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_ALARM_RULE:
      //printf("COMM_ALARM_RULE\n");
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

/***************************************************************************/
/* A listen server for receiving unsolicited events.                       */
/***************************************************************************/
int hik_client::listen_server(string ipAddr, const unsigned int port)
{
  iHandle = NET_DVR_StartListen_V30((char *)ipAddr.c_str(), port, MessageCallback_V30, NULL);
  if ( iHandle < 0 )
  {
    int lError = NET_DVR_GetLastError();
    printf("NET_DVR_StartListen_V30() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
  }

  return iHandle;
}

/***************************************************************************/
/* Add a DVR/NVR device to be monitored.                                   */
/***************************************************************************/
//#define LOGIN_V30
int hik_client::add_source(int devId, string ipAddr, string username, string password)
{
  _dev_info_ camera = {0};

  //---------------------------------------
  // Log in to device
#ifdef LOGIN_V30
  NET_DVR_DEVICEINFO_V30 StruDeviceInfoV30;
  int UserId = NET_DVR_Login_V30((char *)ipAddr.c_str(), 8000, (char *)username.c_str(), (char *)password.c_str(), &StruDeviceInfoV30);
  if (UserId < 0)
  {
    int lError = NET_DVR_GetLastError();
    printf("NET_DVR_Login_V30() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
  }
  else
  {
    long handle = NET_DVR_SetupAlarmChan_V30(UserId);
#else
  //---------------------------------------
  // Prepare to login to device
  NET_DVR_USER_LOGIN_INFO struLoginInfo = {0};
  struLoginInfo.bUseAsynLogin = false;
  struLoginInfo.wPort = 8000;
  struLoginInfo.byLoginMode = 0;   // 0 - SDK Private, 1 = ISAPI  2 = Adaptive (not recommended)
  strncpy(struLoginInfo.sDeviceAddress, ipAddr.c_str(), NET_DVR_DEV_ADDRESS_MAX_LEN);
  strncpy(struLoginInfo.sUserName, username.c_str(), NAME_LEN);
  strncpy(struLoginInfo.sPassword, password.c_str(), NAME_LEN);

  //---------------------------------------
  // Log in to device
  int UserId = NET_DVR_Login_V40(&struLoginInfo, &camera.struDeviceInfoV40);
  if (UserId < 0)
  {
     int lError = NET_DVR_GetLastError();
     printf("NET_DVR_Login_V40() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
  }
  else
  {
    //---------------------------------------
    // Enable Arming Mode V5.1
    NET_DVR_SETUPALARM_PARAM_V50 struSetupAlarmParam = { 0 };
    struSetupAlarmParam.dwSize = sizeof(struSetupAlarmParam);
    struSetupAlarmParam.byLevel = 1;                  // Arming priority: 0-high, 1-medium, 2-low
    struSetupAlarmParam.byAlarmInfoType = 0;          // Intel. traffic alarm type: 0-old (NET_DVR_PLATE_RESULT),1-new (NET_ITS_PLATE_RESULT).
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
 
    long handle = NET_DVR_SetupAlarmChan_V50(UserId, &struSetupAlarmParam, NULL, 0);
#endif
    if (handle < 0)
    {
      int lError = NET_DVR_GetLastError();
      printf("NET_DVR_SetupAlarmChan_Vxx() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
      NET_DVR_Logout(UserId);
      UserId = -1;
    }
    else
    {
      camera.devId  = devId;
      camera.userId = UserId;
      camera.handle = handle;
      lDevices.push_back(camera);
      std::cout << "Device added (devId: " << devId << ", UserId: " << UserId << ", handle: " << handle << ")" << std::endl;
    }
  }

  return UserId;
}

