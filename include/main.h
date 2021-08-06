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
  int     devId;       // Device friendly name
  string  ipAddr;      // Device IP address
  string  username;    // Login username
  string  password;    // Login password
};

class hikmqtt
{
  std::list <_device_> devices;
  string mqtt_server, mqtt_user, mqtt_pass;
  const char *mqtt_sub, *mqtt_pub;
  const char *cfgFile;
  int mqtt_port;
  Config cfg;

  struct command
  {
    char          name[24];
    void          (hikmqtt::*command)(int devId, cJSON *cmdArgs);
  };
  // We only want one of these
  static command command_list[];
public:

  hikmqtt() {};
  ~hikmqtt() {};

  int  read_config(const char *configFile);
  int  run(void);

private:
  void call_preset_num(int devId, cJSON *cmdArgs);
  void delete_preset_num(int devId, cJSON *cmdArgs);
  void get_cur_preset(int devId, cJSON *cmdArgs);
  void get_dvr_info(int devId, cJSON *cmdArgs);
  void ptz_move(int devId, cJSON *cmdArgs);
  void get_preset_details(int devId, cJSON *cmdArgs);
  void get_ptz_pos(int devId, cJSON *cmdArgs);
  void set_preset_num(int devId, cJSON *cmdArgs);
  void update_preset_names(int devId, cJSON *cmdArgs);


  int  load_config();
  void hikdaemon(void);
  int  str_cmp(const char *arg1, const char *arg2);
  int  lookup_command(const char *arg);
  void process_mqtt_cmd(const struct mosquitto_message *message);

  static void on_message(const struct mosquitto_message *message, void *ptr);
};

#endif //HIKMQTT_H

