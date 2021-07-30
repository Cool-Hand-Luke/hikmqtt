#ifndef HIKMQTT_MQTT_H
#define HIKMQTT_MQTT_H

#include <cstring>
#include <cstdio>
#include <mosquittopp.h>

#define MAX_PAYLOAD 256
#define DEFAULT_KEEP_ALIVE 60

class mqtt_client : public mosqpp::mosquittopp
{
  bool m_isConnected, m_bStop;

public:
  mqtt_client(const char* _id, const char *username, const char *password, const char* _host, int _port);
  ~mqtt_client();
  virtual bool send_message(const char *message);

private:
  void on_connect(int rc);
  void on_disconnect(int rc);
  void on_message(const struct mosquitto_message *message);
  void on_subscribe(int mid, int qos_count, const int *granted_qos);
  void on_publish(int rc);
};

#endif //HIKMQTT_MQTT_H
