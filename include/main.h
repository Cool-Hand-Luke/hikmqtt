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
  const char *mqtt_sub, *mqtt_pub;
  int mqtt_port;
  const char *cfgFile;

  struct command
  {
    char          name[24];
    void          (hikmqtt::*command)(const char *devId, const char *data);
  };
  // We only want one of these
  static command command_list[];
public:

  hikmqtt() {};
  ~hikmqtt() {};

  int  read_config(const char *configFile);
  int  run(void);

private:
  void call_ptz_preset(const char *devId, const char *data);
  void get_dev_info(const char *devId, const char *data);

  int  read_config();
  void hikdaemon(void);
  int  str_cmp(const char *arg1, const char *arg2);
  int  lookup_command(const char *arg);
  void process_mqtt_cmd(const struct mosquitto_message *message);

  static void on_message(const struct mosquitto_message *message, void *ptr);
};

#endif //HIKMQTT_H

