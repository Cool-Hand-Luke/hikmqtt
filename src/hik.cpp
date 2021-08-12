

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <cjson/cJSON.h>

#include "HCNetSDK.h"
#include "main.h"
#include "hik.h"

//#define HIK_DEBUG
#define SDK_LOGLEVEL 0

using namespace std;
moodycamel::BlockingConcurrentQueue <char *> *messageQueue;

void CALLBACK MessageCallback_V51(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser);
BOOL CALLBACK MessageCallback_V31(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser);
void CALLBACK ExceptionCallBack(DWORD dwType, LONG lUserId, LONG lHandle, void *pUser);

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
void CALLBACK ExceptionCallBack(DWORD dwType, LONG lUserId, LONG lHandle, void *pUser)
{
  std::cerr << "ExceptionCallBack lUserId: " << lUserId << ", handle: " << lHandle << ", user data: %p" << pUser << std::endl;

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

hik_client::hik_client(moodycamel::BlockingConcurrentQueue <char *> *msgQ)
{
  messageQueue = msgQ;
  init_hik();
}

/***************************************************************************/
/* Report errors to MQTT                                                   */
/***************************************************************************/
void hik_client::simple_report(long devId, int infoT, const char *message, bool status)
{
  cJSON *hikEvent = cJSON_CreateObject();

  cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
  cJSON_AddNumberToObject(hikEvent, "InfoT", infoT);
  cJSON_AddNumberToObject(hikEvent, "Status", status);
  cJSON_AddStringToObject(hikEvent, "msg", message);

  auto msg = strdup(cJSON_Print(hikEvent));
  messageQueue->enqueue(msg);

  cJSON_Delete(hikEvent);
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

  NET_DVR_SDKLOCAL_CFG struSdkLocalCfg = { 0 };
  struSdkLocalCfg.byEnableAbilityParse = 1;
  struSdkLocalCfg.byVoiceComMode = 1;
  struSdkLocalCfg.byLoginWithSimXml = 1;
  NET_DVR_SetSDKLocalConfig( &struSdkLocalCfg );

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
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    DWORD dwReturn = 0;
    NET_DVR_PTZPOS struPtzPos = {0};

    if (!NET_DVR_GetDVRConfig(dev->userId, NET_DVR_GET_PTZPOS, channel, &struPtzPos, sizeof(struPtzPos), &dwReturn))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_GET_PTZPOS, NET_DVR_GetErrorMsg(&lError), false);
    }
    else
    {
      cJSON *hikEvent = cJSON_CreateObject();

      cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
      cJSON_AddNumberToObject(hikEvent, "InfoT", HM_GET_PTZPOS);
      cJSON_AddNumberToObject(hikEvent, "Pan", (int) struPtzPos.wPanPos);
      cJSON_AddNumberToObject(hikEvent, "Tilt", (int) struPtzPos.wTiltPos);
      cJSON_AddNumberToObject(hikEvent, "Zoom", (int) struPtzPos.wZoomPos);

      auto msg = strdup(cJSON_Print(hikEvent));
      messageQueue->enqueue(msg);

      cJSON_Delete(hikEvent);
    }
  }
}
/***************************************************************************/
/* Set the current camera pan, tilt, zoom position.                        */
/***************************************************************************/
void hik_client::set_ptz_pos(int devId, long channel, int pan, int tilt, int zoom)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    NET_DVR_PTZPOS struPtzPos = {0};
    struPtzPos.wPanPos = pan;
    struPtzPos.wTiltPos = tilt;
    struPtzPos.wZoomPos = zoom;
    struPtzPos.wAction = 1;

    if (!NET_DVR_SetDVRConfig(dev->userId, NET_DVR_SET_PTZPOS, channel, &struPtzPos, sizeof(struPtzPos)))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_SET_PTZPOS, NET_DVR_GetErrorMsg(&lError), false);
    }
    else
    {
      cJSON *hikEvent = cJSON_CreateObject();

      cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
      cJSON_AddNumberToObject(hikEvent, "InfoT", HM_SET_PTZPOS);
      cJSON_AddNumberToObject(hikEvent, "Pan", (int) struPtzPos.wPanPos);
      cJSON_AddNumberToObject(hikEvent, "Tilt", (int) struPtzPos.wTiltPos);
      cJSON_AddNumberToObject(hikEvent, "Zoom", (int) struPtzPos.wZoomPos);

      auto msg = strdup(cJSON_Print(hikEvent));
      messageQueue->enqueue(msg);

      cJSON_Delete(hikEvent);
    }
  }
}

/***************************************************************************/
/* Retrieve information about the requested DVR/NVR                        */
/***************************************************************************/
void hik_client::get_dvr_config(int devId)
{
  char tmpBuf[MAX_BUFSIZE+1];

  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    DWORD dwReturn = 0;
    NET_DVR_DEVICECFG_V40 struDevCfg = {0};

    if (!NET_DVR_GetDVRConfig(dev->userId, NET_DVR_GET_DEVICECFG_V40, 0, &struDevCfg, sizeof(NET_DVR_DEVICECFG_V40), &dwReturn))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_GET_DVRCONFIG, NET_DVR_GetErrorMsg(&lError), false);
    }
    else
    {
      memset(tmpBuf, 0, MAX_BUFSIZE);
      cJSON *hikEvent = cJSON_CreateObject();

      cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
      cJSON_AddNumberToObject(hikEvent, "InfoT", HM_DVR_CONFIG);
      cJSON_AddNumberToObject(hikEvent, "DVR ID", (long) struDevCfg.dwDVRID);
      cJSON_AddStringToObject(hikEvent, "DVR Name", (char *) struDevCfg.sDVRName );
      cJSON_AddStringToObject(hikEvent, "Dev Type", (char *) struDevCfg.byDevTypeName );
      cJSON_AddStringToObject(hikEvent, "Serial", (char *) struDevCfg.sSerialNumber);

      sprintf(tmpBuf, "0x%x", struDevCfg.dwHardwareVersion);
      cJSON_AddStringToObject(hikEvent, "HW Ver", tmpBuf);

      if ( ((struDevCfg.dwSoftwareVersion>>24) & 0xFF) > 0 )
      {
        sprintf(tmpBuf, "V%d.%d.%d build %02d%02d%02d", (struDevCfg.dwSoftwareVersion >> 24) & 0xFF,
            (struDevCfg.dwSoftwareVersion >> 16) & 0xFF, struDevCfg.dwSoftwareVersion & 0xFFFF,
            (struDevCfg.dwSoftwareBuildDate >> 16) & 0xFFFF, (struDevCfg.dwSoftwareBuildDate >> 8) & 0xFF,struDevCfg.dwSoftwareBuildDate & 0xFF);
      }
      else
      {
        sprintf(tmpBuf, "V%d.%d build %02d%02d%02d", (struDevCfg.dwSoftwareVersion >> 16) & 0xFFFF, struDevCfg.dwSoftwareVersion & 0xFFFF,
            (struDevCfg.dwSoftwareBuildDate >> 16) & 0xFFFF,
            (struDevCfg.dwSoftwareBuildDate >> 8) & 0xFF, struDevCfg.dwSoftwareBuildDate & 0xFF);
      }
      cJSON_AddStringToObject(hikEvent, "SW Ver", (char *) tmpBuf);

      sprintf(tmpBuf, "V%d.%d build %02d%02d%02d", (struDevCfg.dwDSPSoftwareVersion >> 16) & 0xFFFF, struDevCfg.dwDSPSoftwareVersion & 0xFFFF,
          ((struDevCfg.dwDSPSoftwareBuildDate >> 16) & 0xFFFF) - 2000,
          (struDevCfg.dwDSPSoftwareBuildDate >> 8) & 0xFF, struDevCfg.dwDSPSoftwareBuildDate & 0xFF);
      cJSON_AddStringToObject(hikEvent, "DSP Ver", (char *) tmpBuf);

      cJSON_AddNumberToObject(hikEvent, "Alarm In", (int) struDevCfg.byAlarmInPortNum);
      cJSON_AddNumberToObject(hikEvent, "Alarm Out", (int) struDevCfg.byAlarmOutPortNum);
      cJSON_AddNumberToObject(hikEvent, "Channels", (int) struDevCfg.byChanNum);
      cJSON_AddNumberToObject(hikEvent, "Harddisks", (int) struDevCfg.byDiskNum);

      auto msg = strdup(cJSON_Print(hikEvent));
      messageQueue->enqueue(msg);

      cJSON_Delete(hikEvent);
    }
  }
}

void hik_client::set_dvr_config(int devId, long channel)
{
  //NET_DVR_SetDVRConfig(g_pMainDlg->m_struDeviceInfo.lLoginID, NET_DVR_SET_PTZPOS, 0, &m_ptzPos, sizeof(NET_DVR_PTZPOS));
}

/***************************************************************************/
/* Manually start/stop recoding on devices (notify on err. only)           */
/***************************************************************************/
void hik_client::start_manual_record(int devId, long channel)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    // Not supported by all devices and will default to 0 when unsupported
    // 0 = manual, 1 = alarm, 2 = postback, 3 = signal, 4 = motion, 5 = tamper.
    if (!NET_DVR_StartDVRRecord(devId, channel, 0))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_START_RECORD, NET_DVR_GetErrorMsg(&lError), false);
    }
  }
}
void hik_client::stop_manual_record(int devId, long channel)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    // 0 = manual, 1 = alarm, 2 = postback, 3 = signal, 4 = motion, 5 = tamper.
    if (!NET_DVR_StopDVRRecord(devId, channel))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_STOP_RECORD, NET_DVR_GetErrorMsg(&lError), false);
    }
  }
}
/***************************************************************************/
/* Remotely reboot the device                                              */
/***************************************************************************/
void hik_client::dvr_reboot(int devId)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    if (!NET_DVR_RebootDVR(devId))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_SET_SUPPLIGHT, NET_DVR_GetErrorMsg(&lError), false);
    }
    else
    {
      simple_report(devId, HM_DVR_REBOOT, "", true);
    }
  }
}

/***************************************************************************/
/*                                                                         */
/***************************************************************************/
void hik_client::test_func(int devId, long channel)
{
}

/***************************************************************************/
/*  Get a list of all the presets for the specified device                 */
/***************************************************************************/
void hik_client::update_preset_names(int devId, long channel)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    DWORD dwReturn = 0;

    if (!NET_DVR_GetDVRConfig(dev->userId, NET_DVR_GET_PRESET_NAME, dev->struDeviceInfoV40.struDeviceV30.byStartChan, &dev->struParams, sizeof(NET_DVR_PRESET_NAME) * MAX_PRESET_V40, &dwReturn))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_UPDATE_PRESET_NAMES, NET_DVR_GetErrorMsg(&lError), false);
    }
  }
}

/***************************************************************************/
/*  Return details about a given preset number                             */
/***************************************************************************/
void hik_client::get_preset_details(int devId, long channel, int presetIndx)
{
  _dev_info_ *dev = get_device_byDevId(devId);

  if ( dev && &dev->struParams != NULL )
  {
    int p = presetIndx -1;
    if ( (presetIndx > 0 && presetIndx <= MAX_PRESET_V40) && dev->struParams[p].wPresetNum > 0 )
    {
      cJSON *hikEvent = cJSON_CreateObject();

      cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
      cJSON_AddNumberToObject(hikEvent, "InfoT", HM_PRESET_DETAILS);
      cJSON_AddNumberToObject(hikEvent, "Preset", (int) dev->struParams[p].wPresetNum);
      cJSON_AddStringToObject(hikEvent, "Name", (char *) dev->struParams[p].byName );
      cJSON_AddNumberToObject(hikEvent, "Pan", (int) dev->struParams[p].wPanPos);
      cJSON_AddNumberToObject(hikEvent, "Tilt", (int) dev->struParams[p].wTiltPos);
      cJSON_AddNumberToObject(hikEvent, "Zoom", (int) dev->struParams[p].wZoomPos);

      auto msg = strdup(cJSON_Print(hikEvent));
      messageQueue->enqueue(msg);

      cJSON_Delete(hikEvent);
    }
  }
}

/***************************************************************************/
/*  Return details about a given preset number                             */
/***************************************************************************/
void hik_client::get_preset_byname(int devId, long channel, const char *preset)
{
  _dev_info_ *dev = get_device_byDevId(devId);

  if ( dev && &dev->struParams != NULL )
  {
    int p;
    for (p = 0; p < MAX_PRESET_V40; p++)
    {
      if ( !strcmp(dev->struParams[p].byName, preset) )
      {
        cJSON *hikEvent = cJSON_CreateObject();

        cJSON_AddNumberToObject(hikEvent, "devId", (int) devId);
        cJSON_AddNumberToObject(hikEvent, "InfoT", HM_PRESET_DETAILS);
        cJSON_AddNumberToObject(hikEvent, "Preset", (int) dev->struParams[p].wPresetNum);
        cJSON_AddStringToObject(hikEvent, "Name", (char *) dev->struParams[p].byName );
        cJSON_AddNumberToObject(hikEvent, "Pan", (int) dev->struParams[p].wPanPos);
        cJSON_AddNumberToObject(hikEvent, "Tilt", (int) dev->struParams[p].wTiltPos);
        cJSON_AddNumberToObject(hikEvent, "Zoom", (int) dev->struParams[p].wZoomPos);

        auto msg = strdup(cJSON_Print(hikEvent));
        messageQueue->enqueue(msg);

        cJSON_Delete(hikEvent);

        break;
      }
    }
  }
}

/***************************************************************************/
/*  Set / Call / Clear the specified preset.                               */
/***************************************************************************/
void hik_client::ptz_preset(int devId, long channel, int ptzCmd, int presetIndx)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    if (!NET_DVR_PTZPreset_Other(dev->userId, channel, ptzCmd, presetIndx))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_PTZ_PRESET, NET_DVR_GetErrorMsg(&lError), false);
    }
  }
}

// ==========================================================================
// (NET_DVR_PTZControlWithSpeed_Other & NET_DVR_PTZControl_Other do the same
// thing, I am just choosing to use them differently as it makes more sense
// to me, in this instance)
// ==========================================================================

/***************************************************************************/
/*  Zoom / Focus / Tilt / Pan the specified device.                        */
/***************************************************************************/
void hik_client::ptz_controlwithspeed(int devId, long channel, int cmd, int speed)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    // Control with speed start
    if (!NET_DVR_PTZControlWithSpeed_Other(dev->userId, channel, cmd, 0, speed))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_PTZ_CONTROL, NET_DVR_GetErrorMsg(&lError), false);
    }
    else
    {
      //usleep(1000000);
      usleep(500000);
      // Control with speed end
      NET_DVR_PTZControlWithSpeed_Other(dev->userId, channel, cmd, 1, speed);
    }
  }
}

/***************************************************************************/
/* LIGHT_PWRON / WIPER_PWRON / FAN_PWRON / HEATER_PWRON / AUX_PWRON1       */
/***************************************************************************/
void hik_client::ptz_control(int devId, long channel, int cmd, bool stop)
{
  _dev_info_ *dev = get_device_byDevId(devId);
  if ( dev )
  {
    if (!NET_DVR_PTZControl_Other(dev->userId, channel, cmd, stop))
    {
      int lError = NET_DVR_GetLastError();
      simple_report(devId, HM_PTZ_CONTROL, NET_DVR_GetErrorMsg(&lError), false);
    }
  }
}

// ==========================================================================

/***************************************************************************/
/*  COMM_ALARM_V30 callback.                                               */
/***************************************************************************/
void hik_client::ProcAlarmV30(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen)
{
  int devId = get_device_byUserId(pAlarmer->lUserID)->devId;

  NET_DVR_ALARMINFO_V30 struAlarmInfoV30;
  memcpy(&struAlarmInfoV30, pAlarmInfo, sizeof(NET_DVR_ALARMINFO_V30));
  printf("COMM_ALARM_V30: devID = %d, Alarm type is %d\n", devId, struAlarmInfoV30.dwAlarmType);

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
  int devId = get_device_byUserId(pAlarmer->lUserID)->devId;

  NET_DVR_ALARMINFO_V40 struAlarmInfoV40;
  memcpy(&struAlarmInfoV40, pAlarmInfo, sizeof(NET_DVR_ALARMINFO_V40));

  printf("COMM_ALARM_V40: devId = %d, Alarm type is %d\n", devId, struAlarmInfoV40.struAlarmFixedHeader.dwAlarmType);

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
  printf("COMM_DEV_STATUS_CHANGED: devId = %d\n", devId);
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

  printf("COMM_GISINFO_UPLOAD: devId = %d\n", devId);

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
  /*
  if ( struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.dwCrossDirection )
  {
    cJSON_AddNumberToObject(hikEvent, "dwDir", (int) struVcaRuleAlarm.struRuleInfo.uEventParam.struTraversePlane.dwCrossDirection);
  }
  */
  //cJSON_AddNumberToObject(hikEvent, "dwID", (int) struVcaRuleAlarm.struTargetInfo.dwID);

  auto msg = strdup(cJSON_Print(hikEvent));
  messageQueue->enqueue(msg);

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
      ProcAlarmV30(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_ALARM_V40:
      ProcAlarmV40(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_ALARM_RULE:
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
      procGISInfoAlarm(pAlarmer, pAlarmInfo, dwBufLen);
      break;
    case COMM_DEV_STATUS_CHANGED:
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
int hik_client::add_source(int devId, string ipAddr, string username, string password)
{
  NET_DVR_USER_LOGIN_INFO struLoginInfo = { 0 };
  _dev_info_ camera = {0};
  long lUserId = -1;

  //---------------------------------------
  // Prepare to login to device
  struLoginInfo.bUseAsynLogin = false;
  strncpy(struLoginInfo.sDeviceAddress, ipAddr.c_str(), NET_DVR_DEV_ADDRESS_MAX_LEN);
  strncpy(struLoginInfo.sUserName, username.c_str(), NAME_LEN);
  strncpy(struLoginInfo.sPassword, password.c_str(), NAME_LEN);
  struLoginInfo.wPort = 8000;

  //---------------------------------------
  // Log in to device
  lUserId = NET_DVR_Login_V40(&struLoginInfo, &camera.struDeviceInfoV40);
  if (lUserId < 0)
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
    struSetupAlarmParam.byLevel = 0;                  // Arming priority: 0-high, 1-medium, 2-low
    struSetupAlarmParam.byAlarmInfoType = 0;          // Intel. traffic alarm type: 0-old (NET_DVR_PLATE_RESULT),1-new (NET_ITS_PLATE_RESULT).
    struSetupAlarmParam.byRetAlarmTypeV40 = 1;        // Motion detection, video loss, video tampering, and alarm input alarm information is sent NET_DVR_ALARMINFO_V40
    struSetupAlarmParam.byRetDevInfoVersion = 0;      // Alarm types of CVR: 0 = NET_DVR_ALARMINFO_DEV, 1 = NET_DVR_ALARMINFO_DEV_V40
    struSetupAlarmParam.byRetVQDAlarmType = 0;        // VQD alarm types: 0 = COMM_ALARM_VQD, 1 = COMM_ALARM_VQD_EX
    struSetupAlarmParam.byFaceAlarmDetection = 1;     // 0 = COMM_UPLOAD_FACESNAP_RESULT, 1 = NET_DVR_FACE_DETECTION
    struSetupAlarmParam.bySupport = 0x0c;             // bit0: Whether to upload picture: 0 = yes, 1 = no
                                                      // bit1: Whether to enable ANR: 0 = no, 1 = yes
                                                      // bit4: Whether to upload behavior analysis events of all detection targets: 0 = no, 1 = yes
                                                      // bit5: Whether to enable all-day event or alarm uploading: 0 = no, 1 = yes
    //struSetupAlarmParam.byBrokenNetHttp = 0;          //
    //struSetupAlarmParam.byDeployType = 0;             // Arming type: 0 = arm via client software, 1 = real-time arming.
    //struSetupAlarmParam.bySubScription = 0;           // Bit7: Whether to upload picture after subscribing motion detection: 0 = no, 1 = yes
 
    long handle = NET_DVR_SetupAlarmChan_V50(lUserId, &struSetupAlarmParam, NULL, 0);

    if (handle < 0)
    {
      int lError = NET_DVR_GetLastError();
      printf("NET_DVR_SetupAlarmChan_Vxx() Error: %d %s\n", lError, NET_DVR_GetErrorMsg(&lError));
      NET_DVR_Logout(lUserId);
      lUserId = -1;
    }
    else
    {
      camera.devId  = devId;
      camera.userId = lUserId;
      camera.handle = handle;
      lDevices.push_back(camera);
      std::cout << "Device added (devId: " << devId << ", UserId: " << lUserId << ", handle: " << handle << ")" << std::endl;
    }
  }

  return lUserId;
}

