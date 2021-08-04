

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <queue>
#include <functional>
#include <type_traits>
#include <cjson/cJSON.h>

#include "HCNetSDK.h"
#include "main.h"
#include "mqtt.h"
#include "hik.h"

using namespace std;

#define CONFIG_FILE "/home/matt/.hikmqtt.cfg"

class hikmqtt *hm;
class mqtt_client *mqtt;
class hik_client *hikc;

// TODO: Thread safe queue
std::queue <char *> msgQueue;

// Easy to use list of commands we respond and the associated function
// Caveat: Needs to be in alphabetical order!
hikmqtt::command hikmqtt::command_list[] = {
  { "call_preset",        &hikmqtt::call_ptz_preset },
  { "delete_preset",      &hikmqtt::delete_ptz_preset },
  { "get_dev_info",       &hikmqtt::get_dev_info },
  { "get_ptz_pos",        &hikmqtt::get_ptz_pos },
  { "set_preset",         &hikmqtt::set_ptz_preset },
};
#define LCTOP ((sizeof(hikmqtt::command_list) / sizeof(struct hikmqtt::command)) -1)

/*********************************************************************************/
// * Program Proper
/*********************************************************************************/
void signalHandler( int signum )
{
  // Cleanup
  delete(hikc);

  // Exit
  exit(signum);
}

void hikmqtt::set_ptz_preset(int devId, cJSON *cmdArgs)
{
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  if ( cJSON_IsNumber(preset) && cJSON_IsNumber(channel) )
  {
    hikc->ptz_preset(devId, channel->valueint, SET_PRESET, preset->valueint);
  }
}
void hikmqtt::call_ptz_preset(int devId, cJSON *cmdArgs)
{
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  if ( cJSON_IsNumber(preset) && cJSON_IsNumber(channel) )
  {
    hikc->ptz_preset(devId, channel->valueint, GOTO_PRESET, preset->valueint);
  }
}
void hikmqtt::delete_ptz_preset(int devId, cJSON *cmdArgs)
{
  cJSON *preset  = cJSON_GetObjectItem(cmdArgs,"preset");
  cJSON *channel = cJSON_GetObjectItem(cmdArgs,"channel");
  if ( cJSON_IsNumber(preset) && cJSON_IsNumber(channel) )
  {
    hikc->ptz_preset(devId, channel->valueint, CLE_PRESET, preset->valueint);
  }
}
void hikmqtt::get_dev_info(int devId, cJSON *cmdArgs)
{
  //std::cout << devId << data << std::endl;
}
void hikmqtt::get_ptz_pos(int devId, cJSON *cmdArgs)
{
  //std::cout << devId << data << std::endl;
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

void hikmqtt::process_mqtt_cmd(const struct mosquitto_message *message)
{
  int payload_size = MAX_PAYLOAD + 1;
  int rc;
  char buf[payload_size];

  // We are only interested in 'hikctrl' messages
  if(!strcmp(message->topic, mqtt_sub))
  {
    memset(buf, 0, payload_size * sizeof(char));

    /* Copy N-1 bytes to ensure always 0 terminated. */
    memcpy(buf, message->payload, MAX_PAYLOAD * sizeof(char));

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
void hikmqtt::on_message(const struct mosquitto_message *message, void *userData)
{
  hikmqtt *ptr = (hikmqtt *)userData;
  ptr->process_mqtt_cmd(message);
}

/*********************************************************************************/
int hikmqtt::read_config()
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
int hikmqtt::read_config(const char *configFile)
{
  int rc;
  _device_ device;

  cfgFile = configFile;

  rc = read_config();
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
int hikmqtt::run(void)
{
  int rc = 0;

  // Connect to our MQTT server
  mqtt = new mqtt_client("hikmqtt", mqtt_user, mqtt_pass, mqtt_server, mqtt_port);
  mqtt->on_message(on_message, this);
  mqtt->sub(mqtt_sub);

  // Initialise our Hikvision class
  hikc = new hik_client(&msgQueue);

  // Become an Alarm Host (create a listener)
  //hikc->listen_server("0.0.0.0", 7200);

  // Iterate through our cameras to start receiving notifications
  std::list <_device_>::iterator it;
  for (it = devices.begin(); it != devices.end(); it++)
  {
    rc = hikc->add_source(it->devId, it->ipAddr, it->username, it->password);
  }

  while(1)
  {
    rc = mqtt->loop();
    if ( rc )
    {
      switch (rc)
      {
      case 4:
        std::cout << "Authentication Error: " << rc << std::endl;
        break;
      case 5:
        std::cout << "Connection Refused: " << rc << std::endl;
        mqtt->reconnect();
        break;
      default:
        std::cout << "Reconnecting on Code: " << rc << std::endl;
        break;
      }

      mqtt->reconnect();
      sleep(1);
    }

    while ( !msgQueue.empty() )
    {
      const char *alert = msgQueue.front();
      std::cout << "alert: " << alert << std::endl;
      mqtt->pub(mqtt_pub, alert);
      msgQueue.pop();
    }

    usleep(20);
  }
  return rc;
}

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
