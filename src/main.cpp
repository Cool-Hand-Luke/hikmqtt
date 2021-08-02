

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <libconfig.h++>
#include <csignal>
#include <thread>
#include <queue>
#include <cjson/cJSON.h>

#include "HCNetSDK.h"
#include "main.h"
#include "mqtt.h"
#include "hik.h"

using namespace std;
using namespace libconfig;

#define CONFIG_FILE "/home/matt/.hikmqtt.cfg"

class hikmqtt *hm;
class mqtt_client *mqtt;
class hik_client *hikc;

// TODO: Thread safe queue
std::queue <char *> msgQueue;
Config cfg;

void signalHandler( int signum )
{
   // Cleanup
   delete(hikc);

   // Exit
   exit(signum);
}

void hikmqtt::on_message(const struct mosquitto_message *message)
{
  int payload_size = MAX_PAYLOAD + 1;
  char buf[payload_size];

  std::cout << "Got message->topic: " <<  message->topic << endl;

  // We are only interested in 'hikctrl' messages
  if(!strcmp(message->topic, "BTRGB/hikctrl"))
  {
    memset(buf, 0, payload_size * sizeof(char));

    /* Copy N-1 bytes to ensure always 0 terminated. */
    memcpy(buf, message->payload, MAX_PAYLOAD * sizeof(char));

    std::cout << "Command: " <<  message->topic << " -> " <<  buf << endl;
  }
}

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

int hikmqtt::run(void)
{
  int rc = 0;

  // Connect to our MQTT server
  mqtt = new mqtt_client("hikmqtt", mqtt_user, mqtt_pass, mqtt_server, mqtt_port);
  mqtt->on_message(on_message);

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

  while(1)
  {
    rc = mqtt->loop();
    if ( rc )
    {
      switch (rc)
      {
      case 4:
        std::cout << "Authentication Error: " << rc << endl;
        break;
      case 5:
        std::cout << "Connection Refused: " << rc << endl;
        mqtt->reconnect();
        break;
      default:
        std::cout << "Reconnecting on Code: " << rc << endl;
        break;
      }

      mqtt->reconnect();
      sleep(1);
    }

    while ( !msgQueue.empty() )
    {
      char *alert = msgQueue.front();
      std::cout << "alert: " << alert << endl;
      mqtt->publish(NULL, "BTRGB/hikAlarm", strlen(alert), alert, /*qos*/0, /*retain*/false);
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
}
