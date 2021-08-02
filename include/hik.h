#ifndef HIKCLIENT_HIK_H
#define HIKCLIENT_HIK_H

#include <cstdio>
#include <string>
#include <string.h>
#include <list>
#include <queue>
#include <mosquittopp.h>

#include "mqtt.h" 

using namespace std;

struct _dev_info_ {
  char  *devId;
  int   userID;
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
  hik_client(std::queue <char *> *msgQ);
  hik_client() {};
  // Destruct
  ~hik_client();

  int add_source(string devId, string ipAddr, string username, string password);
  int listen_server(string ipAddr, const unsigned int port);

private:
  //void CALLBACK MessageCallback(LONG lCommand, NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen, void* pUser);
  void init_hik();
  void SDK_Version();
  void ProcAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcAlarmV30(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcAlarmV40(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcessEventWithJsonDataNoBoundary(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void procGISInfoUpload(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcRuleAlarm(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
};

#endif //HIKCLIENT_HIK_H

