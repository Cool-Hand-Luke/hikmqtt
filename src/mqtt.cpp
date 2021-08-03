
#include <string.h>
#include <iostream>

#include "mqtt.h"

mqtt_client::mqtt_client(const char *id, string username, string password, string host, int port) : mosquittopp(id)
{
  mosqpp::lib_init();

  // Nullify callbacks
  msgCallback = NULL;

  int rc = username_pw_set(username.c_str(), password.c_str());
  if( MOSQ_ERR_SUCCESS != rc )
  {
    printf("[Subscriber::Subscriber] username_pw_set failed\n");
  }
  connect_async(host.c_str(), port, 0);

  this->loop_start();

  printf("MQTT CLIENT STARTED\n");
}

mqtt_client::~mqtt_client()
{
  this->loop_stop();
  mosqpp::lib_cleanup();
}

void mqtt_client::on_message(OnMessage Callback, void *_userData)
{
  msgCallback = Callback;
  userData = _userData;
}

void mqtt_client::on_connect(int rc)
{
  if( rc == MOSQ_ERR_SUCCESS )
  {
    m_isConnected = true;
  }
  else
  {
    printf("[mqtt::on_connect] code: %d\n", rc);
  }
}
void mqtt_client::on_disconnect(int rc)
{
  m_isConnected = false;
  printf("[mqtt::on_disconnect] code: %d\n", rc);
}

void mqtt_client::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
  printf("Subscription succeeded: %d, %d\n", mid, qos_count);
}

void mqtt_client::on_message(const struct mosquitto_message *message)
{
  if ( msgCallback )
  {
    msgCallback(message, userData);
  }
}

void mqtt_client::sub(const char *topic)
{
  subscribe(NULL, topic, 0);
}

bool mqtt_client::pub(const char *topic, const char *_message)
{
  int ret = publish(NULL, topic, strlen(_message), _message,/*qos*/0,/*retain*/false);

  return (ret == MOSQ_ERR_SUCCESS);
}

void mqtt_client::on_publish(int rc)
{
  //printf("[mqtt::on_publish] code: %d\n", rc);
};
