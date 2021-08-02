#ifndef HIKMQTT_H
#define HIKMQTT_H

#include <cstdio>
#include <cstdlib>
#include <libconfig.h++>
#include <list>
#include "mqtt.h"

using namespace std;
using namespace libconfig;

struct _device_
{
  string  devId;       // Device friendly name
  string  ipAddr;      // Device IP address
  string  username;    // Login username
  string  password;    // Login password
};

class hikmqtt
{
  Config cfg;
  std::list <_device_> devices;
  string mqtt_server, mqtt_user, mqtt_pass;
  const char *mqtt_pub;
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



