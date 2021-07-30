

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

class mqtt_client *mqtt;
class hik_client *hikc;

// TODO: Thread safe queue
std::queue <char *> msgQueue;

void signalHandler( int signum )
{
   // Cleanup
   delete(hikc);

   // Exit
   exit(signum);
}

// TODO: Config file
struct _cameras_
{
  const char    *devId;        // Device friendly name
  const char    *ipAddr;      // Device IP address
  const char    *username;    // Login username
  const char    *password;    // Login password
} cameras[] = {
    {"1",     "10.10.10.19",   "admin",  "admin"},
};
#define CTOP (int)((sizeof(cameras) / sizeof(struct _cameras_)) -1)

int main(void)
{
  int rc;

  // register signal SIGINT and signal handler
  signal(SIGINT, signalHandler); 

  // Connect to our MQTT server
  mqtt = new mqtt_client("hikmqtt", "hikmqtt", "passwd", "10,10.10.12", 1883);

  // Initialise our Hikvision class
  hikc = new hik_client(&msgQueue);

  // Become an Alarm Host (create a listener)
  //hikc->listen_server("0.0.0.0", 7200);

  // Iterate through our cameras to start receiving notifications
  for ( int i = 0; i <=  CTOP; i++ )
  {
    hikc->add_source(cameras[i].devId, cameras[i].ipAddr, cameras[i].username, cameras[i].password);
  }

  while(1)
  {
    rc = mqtt->loop();
    if ( rc )
    {
      switch (rc)
      {
      case 4:
        std::cout << "Authentication Error " << rc << std::endl;
        break;
      case 5:
        std::cout << "Connection Refused " << rc << std::endl;
        mqtt->reconnect();
        break;
      default:
        std::cout << "reconnecting on code " << rc << std::endl;
        break;
      }

      mqtt->reconnect();
      sleep(1);
    }

    while ( !msgQueue.empty() )
    {
      char *alert = msgQueue.front();
      cout << alert << endl;
      mqtt->publish(NULL, "BTRGB/hikAlarm", strlen(alert), alert, /*qos*/0, /*retain*/false);
      msgQueue.pop();
    }

    usleep(20);
  }
}
