#include "mqtt.h"
#include <iostream>

mqtt_client::mqtt_client(const char *id, const char *username, const char *password, const char* host, int port) : mosquittopp(id)
{
  mosqpp::lib_init();

  int rc = username_pw_set(username, password);
  if( MOSQ_ERR_SUCCESS != rc )
  {
    std::cout << "[Subscriber::Subscriber] username_pw_set failed" << std::endl;
  }
  connect_async(host, port, 0);

  this->loop_start();

  std::cout << "MQTT CLIENT STARTED" << std::endl;
}

mqtt_client::~mqtt_client()
{
  this->loop_stop();
  mosqpp::lib_cleanup();
}

void mqtt_client::on_connect(int rc)
{
  if( rc == MOSQ_ERR_SUCCESS )
  {
    m_isConnected = true;
    subscribe(NULL, "BTRGB/#", 0);
  }
  else
  {
    std::cout << "[mqtt::on_connect] code " << rc << std::endl;
  }
}
void mqtt_client::on_disconnect(int rc)
{
  m_isConnected = false;
  std::cout << "[mqtt::on_disconnect] code " << rc << std::endl;
}

void mqtt_client::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
  std::cout << "Subscription succeeded. " << mid << " " << qos_count << std::endl;
}

void mqtt_client::on_message(const struct mosquitto_message *message)
{
  int payload_size = MAX_PAYLOAD + 1;
  char buf[payload_size];

  //std::cout << "Got message->topic " << message->topic << std::endl;

  // We are only interested in 'hikmqtt' messages
  if(!strcmp(message->topic, "BTRGB/hikmqtt"))
  {
    memset(buf, 0, payload_size * sizeof(char));

    /* Copy N-1 bytes to ensure always 0 terminated. */
    memcpy(buf, message->payload, MAX_PAYLOAD * sizeof(char));

    std::cout << message->topic << " -> " << buf << std::endl;
  }
}

bool mqtt_client::send_message(const char* _message) 
{
  int ret = publish(NULL, "BTRGB/hikAlarm", strlen(_message), _message, 1, false);

  return (ret == MOSQ_ERR_SUCCESS);
}

void mqtt_client::on_publish(int rc)
{
  //std::cout << "[mqtt::on_publish] code " << rc << std::endl;
};
