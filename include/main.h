#ifndef HIKMQTT_H
#define HIKMQTT_H

#include <cstdio>
#include <cstdlib>
#include <list>
#include <string>
#include <mosquitto.h>
#include <libconfig.h++>

#include "hik.h"

using namespace std;
using std::string;
using namespace libconfig;

#define  MAX_BUFSIZE   512
struct _device_
{
  int     devId;       // Device friendly name
  string  ipAddr;      // Device IP address
  string  username;    // Login username
  string  password;    // Login password
};

class hikmqtt
{
  bool   m_loop;
  class  hik_client *hikc;
  struct mosquitto *mqtt;
  const char *mqtt_sub, *mqtt_pub;
  string cfgFile;
  int mqtt_port;
  std::list <_device_> devices;
  string mqtt_server, mqtt_user, mqtt_pass;
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
  ~hikmqtt();

  int  read_config(string &configFile);
  void run(void);

private:
  void call_preset_num(int devId, cJSON *cmdArgs);
  void delete_preset_num(int devId, cJSON *cmdArgs);
  void get_cur_preset(int devId, cJSON *cmdArgs);
  void get_dvr_info(int devId, cJSON *cmdArgs);
  void ptz_move(int devId, cJSON *cmdArgs);
  void get_preset_details(int devId, cJSON *cmdArgs);
  void get_preset_byname(int devId, cJSON *cmdArgs);
  void get_ptz_pos(int devId, cJSON *cmdArgs);
  void set_ptz_pos(int devId, cJSON *cmdArgs);
  void set_preset_num(int devId, cJSON *cmdArgs);
  void dvr_reboot(int devId, cJSON *cmdArgs);
  void start_manual_record(int devId, cJSON *cmdArgs);
  void stop_manual_record(int devId, cJSON *cmdArgs);
  void update_preset_names(int devId, cJSON *cmdArgs);
  void test_func(int devId, cJSON *cmdArgs);


  string get_configFile(void);
  int  load_config();
  void hikdaemon(void);
  int  str_cmp(const char *arg1, const char *arg2);
  int  lookup_command(const char *arg);
  void process_mqtt_cmd(const struct mosquitto_message *message);

  static void mqtt_callback(mosquitto *mosq, void *obj, const mosquitto_message *message);
};

#endif //HIKMQTT_H

