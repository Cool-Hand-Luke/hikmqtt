#ifndef HIKMQTT_H
#define HIKMQTT_H

#include <cstdio>
#include <cstdlib>
#include <list>
#include "mqtt.h"

using namespace std;

struct _device_
{
  string  devId;       // Device friendly name
  string  ipAddr;      // Device IP address
  string  username;    // Login username
  string  password;    // Login password
};

class hikmqtt
{
  std::list <_device_> devices;
  string mqtt_server, mqtt_user, mqtt_pass;
  int mqtt_port;
  const char *cfgFile;

public:
  hikmqtt() {};
   ~hikmqtt();

  int read_config(const char *configFile);
  int run(void);
private:
  int  read_config();
  void hikdaemon(void);
  static void on_message(const struct mosquitto_message *message);
};

#endif //HIKMQTT_H



