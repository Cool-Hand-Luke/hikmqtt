#ifndef HIKCLIENT_HIK_H
#define HIKCLIENT_HIK_H

#include <cstdio>
#include <string>
#include <string.h>
#include <list>
#include <mosquittopp.h>

#include "blockingconcurrentqueue.h"

using namespace std;

enum _info_t_ {
  INFO_DVR_CONFIG = 0
};

struct _dev_info_ {
  int   devId;
  DWORD  userId;
  DWORD  handle;
  NET_DVR_DEVICEINFO_V40 struDeviceInfoV40;
  NET_DVR_PRESET_NAME struParams[MAX_PRESET_V40];
};

class hik_client
{
  int iHandle = -1;
  std::list<_dev_info_> lDevices;

  NET_DVR_TRACK_MODE m_struLFTrackMode;
  NET_DVR_TRACK_CFG m_struLFCfg;
  NET_DVR_CB_POINT m_struCurCBPPoint;
  NET_DVR_PU_STREAM_CFG m_struPUStream;

public:
  // Construct
  hik_client(moodycamel::BlockingConcurrentQueue <char *> *msgQ);
  hik_client() {};
  // Destruct
  ~hik_client();

  int  add_source(int devId, string ipAddr, string username, string password);
  int  listen_server(string ipAddr, const unsigned int port);
  void proc_callback_message(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);

  void get_dvr_config(int devId, long channel);
  void set_dvr_config(int devId, long channel);

  int  update_preset_names(int devId, long channel);
  void get_ptz_pos(int devId, long channel);
  void ptz_preset(int devId, long channel, int ptzCmd, int presetIndx);
  void get_preset_details(int devId, long channel, int presetIndx);

  void ptz_controlwithspeed(int devId, long channel, int dir, int speed);

private:
  void init_hik();
  void SDK_Version();
  void ProcDevStatusChanged(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void procGISInfoAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);

  _dev_info_ *get_device_byDevId(int);
  _dev_info_ *get_device_byUserId(DWORD);
  _dev_info_ *get_device_byHandle(DWORD);

  void ProcAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcAlarmV30(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcAlarmV40(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcessEventWithJsonDataNoBoundary(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void procGISInfoUpload(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcRuleAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
};

#endif //HIKCLIENT_HIK_H

