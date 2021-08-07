
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
    std::cout << "[Subscriber::Subscriber] username_pw_set failed" << std::endl;
  }
  connect_async(host.c_str(), port, 0);

  this->loop_start();

  std::cout << "MQTT CLIENT STARTED" << std::endl;
}

mqtt_client::~mqtt_client()
{
  this->loop_stop();
  mosqpp::lib_cleanup();
}

void mqtt_client::set_callback(OnMessage Callback, void *_userData)
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
    std::cout << "[mqtt::on_connect] code: " << rc << std::endl;
  }
}
void mqtt_client::on_disconnect(int rc)
{
  m_isConnected = false;
  std::cout << "[mqtt::on_disconnect] code: " << rc << std::endl;
}

void mqtt_client::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
  std::cout << "Subscription succeeded: " << mid << ", " << qos_count << std::endl;
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

bool mqtt_client::pub(const char *topic, const char *_message, bool retain)
{
  int ret = publish(NULL, topic, strlen(_message), _message, 0,retain);

  return (ret == MOSQ_ERR_SUCCESS);
}

void mqtt_client::on_publish(int rc)
{
  //std::cout << ("[mqtt::on_publish] code: %d\n", rc);
};
