

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <csignal>
#include <functional>
#include <type_traits>
#include <cjson/cJSON.h>
#include <mosquitto.h>

#include "HCNetSDK.h"
#include "main.h"
#include "hik.h"
#include "blockingconcurrentqueue.h"

using namespace std;

#define CONFIG_FILE "/home/matt/.hikmqtt.cfg"

class  hikmqtt *hm;
//class  MqttClient *mqtt;
struct mosquitto *mqtt;
class  hik_client *hikc;

moodycamel::BlockingConcurrentQueue <char *> msgQueue;

/*********************************************************************************/
/* Easy to use list of commands we respond and the associated function           */
/* Caveat: Needs to be in alphabetical order!                                    */
/*********************************************************************************/
hikmqtt::command hikmqtt::command_list[] = {
  { "call_preset",             &hikmqtt::call_preset_num },
  { "delete_preset",           &hikmqtt::delete_preset_num },
  { "get_dvr_info",            &hikmqtt::get_dvr_info },
  { "get_preset_byname",       &hikmqtt::get_preset_byname },
  { "get_preset_details",      &hikmqtt::get_preset_details },
  { "get_ptz_pos",             &hikmqtt::get_ptz_pos },
  { "ptz_move",                &hikmqtt::ptz_move },
  { "set_preset",              &hikmqtt::set_preset_num },
  { "set_ptz_pos",             &hikmqtt::set_ptz_pos },
  { "set_supplementlight",     &hikmqtt::set_supplementlight },
  { "start_record",            &hikmqtt::start_manual_record },
  { "stop_record",             &hikmqtt::stop_manual_record },
  { "update_preset_names",     &hikmqtt::update_preset_names },
};
#define LCTOP ((sizeof(hikmqtt::command_list) / sizeof(struct hikmqtt::command)) -1)

/*********************************************************************************/
/* Signal handler to clean up on forced exit                                     */
/*********************************************************************************/
void signalHandler( int signum )
{
  // Cleanup
  delete(hikc);

  // Exit
  exit(signum);
}

/*********************************************************************************/
/* Request to update our list of preset names (does nothing else)                */
/*********************************************************************************/
void hikmqtt::update_preset_names(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  if ( cJSON_IsNumber(channel) )
  {
    hikc->update_preset_names(devId, channel->valueint);
  }
}
/*********************************************************************************/
/* Set the current view as preset (x) for specified device.                      */
/*********************************************************************************/
void hikmqtt::set_preset_num(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  if ( cJSON_IsNumber(channel) && cJSON_IsNumber(preset) )
  {
    hikc->ptz_preset(devId, channel->valueint, SET_PRESET, preset->valueint);
  }
}
/*********************************************************************************/
/* Change the view of the specified device to preset (x)                         */
/*********************************************************************************/
void hikmqtt::call_preset_num(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  if ( cJSON_IsNumber(channel) && cJSON_IsNumber(preset) )
  {
    hikc->ptz_preset(devId, channel->valueint, GOTO_PRESET, preset->valueint);
  }
}
/*********************************************************************************/
/* Delete the specified preset on the specified device.                          */
/*********************************************************************************/
void hikmqtt::delete_preset_num(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  if ( cJSON_IsNumber(channel) && cJSON_IsNumber(preset) )
  {
    hikc->ptz_preset(devId, channel->valueint, CLE_PRESET, preset->valueint);
  }
}
/*********************************************************************************/
/* Get some details of the specified device.                                     */
/*********************************************************************************/
void hikmqtt::get_dvr_info(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  if ( cJSON_IsNumber(channel) )
  {
    hikc->get_dvr_config(devId, channel->valueint);
  }
}
/*********************************************************************************/
/*                                                                               */
/*********************************************************************************/
void hikmqtt::set_supplementlight(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  if ( cJSON_IsNumber(channel) )
  {
    hikc->set_supplementlight(devId, channel->valueint);
  }
}
/*********************************************************************************/
/* Get the pan, tilt, zoom, etc of the specified device.                         */
/*********************************************************************************/
void hikmqtt::get_ptz_pos(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  if ( cJSON_IsNumber(channel) && cJSON_IsNumber(preset) )
  {
    hikc->get_ptz_pos(devId, channel->valueint);
  }
}
/*********************************************************************************/
/* Get the pan, tilt, zoom, etc of the specified device.                         */
/*********************************************************************************/
void hikmqtt::start_manual_record(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  if ( cJSON_IsNumber(channel) )
  {
    hikc->start_manual_record(devId, channel->valueint);
  }
}
/*********************************************************************************/
/* Get the pan, tilt, zoom, etc of the specified device.                         */
/*********************************************************************************/
void hikmqtt::stop_manual_record(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  if ( cJSON_IsNumber(channel) )
  {
    hikc->stop_manual_record(devId, channel->valueint);
  }
}
/*********************************************************************************/
/* Set the pan, tilt, zoom of the specified device.                         */
/*********************************************************************************/
void hikmqtt::set_ptz_pos(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  cJSON *pan     = cJSON_GetObjectItem(cmdArgs,"pan");
  cJSON *tilt    = cJSON_GetObjectItem(cmdArgs,"tilt");
  cJSON *zoom    = cJSON_GetObjectItem(cmdArgs,"zoom");

  if ( cJSON_IsNumber(channel) && cJSON_IsNumber(pan) && cJSON_IsNumber(tilt) && cJSON_IsNumber(zoom) )
  {
    hikc->set_ptz_pos(devId, channel->valueint, pan->valueint, tilt->valueint, zoom->valueint);
  }
}
/*********************************************************************************/
/* Get the name, pan, tilt, zoom, values of the requested preset.                */
/*********************************************************************************/
void hikmqtt::get_preset_details(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  if ( cJSON_IsNumber(channel) && cJSON_IsNumber(preset) )
  {
    hikc->get_preset_details(devId, channel->valueint, preset->valueint);
  }
}
/*********************************************************************************/
/* Get the name, pan, tilt, zoom, values of the requested preset.                */
/*********************************************************************************/
void hikmqtt::get_preset_byname(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  if ( cJSON_IsNumber(channel) && cJSON_IsString(preset) && (preset->valuestring != NULL) )
  {
    hikc->get_preset_byname(devId, channel->valueint, preset->valuestring);
  }
}
/*********************************************************************************/
/* Move the specified device.                                                    */
/*********************************************************************************/
void hikmqtt::ptz_move(int devId, cJSON *cmdArgs)
{
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  cJSON *direct  = cJSON_GetObjectItem(cmdArgs,"direction");
  if ( cJSON_IsNumber(channel) && cJSON_IsNumber(direct) )
  {
    hikc->ptz_controlwithspeed(devId, channel->valueint, PAN_LEFT, 2);
  }
}

/*********************************************************************************/
/*  These following string comparers are CASE INSENSITIVE and will only compare  */
/*  Alphabetic characters reliably.                                              */
/*********************************************************************************/
/*   Scan until different or until the end of both  */
int hikmqtt::str_cmp(const char *arg1, const char *arg2)
{
  int  chk;

  while(*arg1 || *arg2)
  {
    if((chk = (*arg1++ | 32) - (*arg2++ | 32)))
    {
      return (chk);
    }
  }

  return(0);
}

/*********************************************************************************/
/* Lookup the passed command with an efficient binary search                     */
/*********************************************************************************/
int hikmqtt::lookup_command(const char *arg)
{
  int top=LCTOP;
  int mid,found,low=0;

  while ( low <= top )
  {
    mid = (low+top) / 2;

    if ( (found = str_cmp(arg,command_list[mid].name)) > 0 )
      low = mid +1 ;
    else if(found < 0)
      top = mid -1;
    else
      return(mid);
  }

  return(-1);
}

/*********************************************************************************/
/* Process MQTT messages for commands to respond too.                            */
/*********************************************************************************/
void hikmqtt::process_mqtt_cmd(const struct mosquitto_message *message)
{
  int rc;

  // We are only interested in 'hikctrl' messages
  if( !strcmp(message->topic, mqtt_sub) )
  {
    cJSON *root = cJSON_Parse((const char *)message->payload);
    if ( root != NULL )
    {
      cJSON *cmdName = cJSON_GetObjectItem(root, "command");
      cJSON *devId   = cJSON_GetObjectItem(root, "devId");
      cJSON *cmdArgs = cJSON_GetObjectItem(root, "args");

      // Ensure we have a command and a numeric device ID
      if ( (cJSON_IsString(cmdName) && (cmdName->valuestring != NULL)) && cJSON_IsNumber(devId) )
      {
        // See if we have a command that matches the request
        rc = lookup_command((const char *)cmdName->valuestring);
        if ( rc >= 0 )
        {
          // Damn, this made my life easy. If only I had known of it sooner! :)
          std::invoke(command_list[rc].command, this, devId->valueint, cmdArgs);
        }
      }
      cJSON_Delete(root);
    }
  }
}

/*********************************************************************************/
/* Our MQTT callback handler.                                                    */
/*********************************************************************************/
void hikmqtt::mqtt_callback(mosquitto *mosq, void *userData, const mosquitto_message *message)
{
  hikmqtt *ptr = (hikmqtt *)userData;
  ptr->process_mqtt_cmd(message);
}

/*********************************************************************************/
/* Load the config file from disk and check for syntax errors.                   */
/*********************************************************************************/
int hikmqtt::load_config()
{
  try
  {
    cfg.readFile("/home/matt/.hikmqtt.cfg");
  }

  catch(const FileIOException &fioex)
  {
    std::cerr << "I/O error while reading file." << std::endl;
    return(EXIT_FAILURE);
  }
  catch(const ParseException &pex)
  {
    std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine() << " - " << pex.getError() << std::endl;
    return(EXIT_FAILURE);
  }

  return 0;
}

/*********************************************************************************/
/* Load and process the config file for our dynamic settings...                  */
/*********************************************************************************/
int hikmqtt::read_config(const char *configFile)
{
  int rc;
  _device_ device;

  cfgFile = configFile;

  rc = load_config();
  if (!rc)
  {
    const Setting &root = cfg.getRoot();
    try
    {
      const Setting &mqtt_settings = root["mqtt_server"];
      mqtt_settings.lookupValue("address", mqtt_server);
      mqtt_settings.lookupValue("username", mqtt_user);
      mqtt_settings.lookupValue("password", mqtt_pass);
      mqtt_settings.lookupValue("port", mqtt_port);

      mqtt_settings.lookupValue("subscribe", mqtt_sub);
      mqtt_settings.lookupValue("publish", mqtt_pub);

      const Setting &device_settings = root["devices"];
      for ( int i = 0; i < device_settings.getLength(); i++ )
      {
        const Setting &dev = device_settings[i];
        if (!(dev.lookupValue("id", device.devId) &&
              dev.lookupValue("address", device.ipAddr) &&
              dev.lookupValue("username", device.username) &&
              dev.lookupValue("password", device.password)))
        {
          continue;
        }
        else
        {
          devices.push_back(device);
        }
      }
    }
    catch(const SettingNotFoundException &nfex)
    {
      rc = -1;
    }
  }

  return rc;
}

/*********************************************************************************/
/* Our main program loop (where all the actions happens)                         */
/*********************************************************************************/
void hikmqtt::run(void)
{
  char *alert;

  // Initialise MQTT
  mosquitto_lib_init();
  mqtt = mosquitto_new("hikalarm", true, this);
  mosquitto_username_pw_set(mqtt, mqtt_user.c_str(), mqtt_pass.c_str());
  mosquitto_message_callback_set(mqtt, mqtt_callback);
  mosquitto_connect(mqtt,mqtt_server.c_str(), mqtt_port, MOSQ_ERR_KEEPALIVE);
  mosquitto_subscribe(mqtt, NULL, mqtt_sub, 0);
  mosquitto_loop_start(mqtt);

  // Initialise our Hikvision class
  hikc = new hik_client(&msgQueue);

  // Become an Alarm Host (create a listener)
  //hikc->listen_server("0.0.0.0", 7200);

  // Iterate through our cameras to start receiving notifications
  std::list <_device_>::iterator it;
  for (it = devices.begin(); it != devices.end(); it++)
  {
    hikc->add_source(it->devId, it->ipAddr, it->username, it->password);
  }

  // Sit and wait for commands to follow
  while(1)
  {
    msgQueue.wait_dequeue(alert);
    mosquitto_publish(mqtt, NULL, mqtt_pub, strlen(alert), alert, 0, 0 );

    // Cleanup
    free(alert);
  }
  // We may get here one day...
}

/*********************************************************************************/
/* The Program Start...                                                          */
/*********************************************************************************/
int main(void)
{
  int rc;

  // register signal SIGINT and signal handler
  signal(SIGINT, signalHandler); 

  hm = new hikmqtt();
  if (!(rc = hm->read_config(CONFIG_FILE)))
  {
    hm->run();
  }

  return rc;
}
