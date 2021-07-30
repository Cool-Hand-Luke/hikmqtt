#ifndef HIKCLIENT_HIK_H
#define HIKCLIENT_HIK_H

#include <cstdio>
#include <string.h>
#include <list>
#include <queue>
#include <mosquittopp.h>

#include "mqtt.h" 

struct _dev_info_ {
  char  *devId;
  int   userID;
}; 

class hik_client
{
  int iHandle = -1;
  std::list<_dev_info_> lDevices;

public:
  // Construct
  hik_client(std::queue <char *> *msgQ);
  hik_client() {};
  // Destruct
  ~hik_client();

  int add_source(const char *devId, const char *ipAddr, const char *username, const char *password);
  int listen_server(const char *ipAddr, const unsigned int port);

private:
  void init_hik();
  void SDK_Version();
  void ProcAlarmV30(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
  void ProcAlarmV40(NET_DVR_ALARMER *pAlarmer, char *pAlarmInfo, DWORD dwBufLen);
};

#endif //HIKCLIENT_HIK_H

