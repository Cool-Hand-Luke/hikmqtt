#ifndef HIKMQTT_MQTT_H
#define HIKMQTT_MQTT_H

#include <cstdio>
#include <cstdlib>
#include <string>
#include <list>
#include <mosquittopp.h>

using namespace std;

#define MAX_BUFSIZE        512

class mqtt_client : public mosqpp::mosquittopp
{
  bool m_isConnected, m_bStop;

  // C++ version of: typedef void (*InputEvent)(const char*)
  using OnMessage = void (*)(const struct mosquitto_message *message, void *);

public:
  mqtt_client(const char *id, string username, string password, string host, int _port);
  ~mqtt_client();

  void sub(const char *topic);
  bool pub(const char *topic, const char *_message, bool retain);
  void set_callback(OnMessage Callback, void *);

private:
  void     *userData;
  OnMessage msgCallback;

  void on_connect(int rc);
  void on_disconnect(int rc);
  void on_message(const struct mosquitto_message *);
  void on_subscribe(int mid, int qos_count, const int *granted_qos);
  void on_publish(int rc);
};

#endif //HIKMQTT_MQTT_H
