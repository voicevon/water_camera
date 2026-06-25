/*
  PubSubClient.h - A simple client for MQTT.
  Nick O'Leary - http://knolleary.net

  [FORKED for ESP32-CAM PSRAM support]
  Changes vs original v2.8:
    - uint16_t bufferSize  →  uint32_t bufferSize  (removes 64KB cap)
    - setBufferSize / getBufferSize parameter/return changed to uint32_t
    - malloc / realloc / free  →  heap_caps_malloc / heap_caps_realloc / heap_caps_free
      with MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, falling back to internal RAM
*/

#ifndef PubSubClient_h
#define PubSubClient_h

#include <Arduino.h>
#include "IPAddress.h"
#include "Client.h"
#include "Stream.h"

#define MQTT_VERSION_3_1      3
#define MQTT_VERSION_3_1_1    4

#ifndef MQTT_VERSION
#define MQTT_VERSION MQTT_VERSION_3_1_1
#endif

// MQTT_MAX_PACKET_SIZE : Maximum packet size. Override with setBufferSize().
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 256
#endif

#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 15
#endif

#ifndef MQTT_SOCKET_TIMEOUT
#define MQTT_SOCKET_TIMEOUT 15
#endif

// Possible values for client.state()
#define MQTT_CONNECTION_TIMEOUT     -4
#define MQTT_CONNECTION_LOST        -3
#define MQTT_CONNECT_FAILED         -2
#define MQTT_DISCONNECTED           -1
#define MQTT_CONNECTED               0
#define MQTT_CONNECT_BAD_PROTOCOL    1
#define MQTT_CONNECT_BAD_CLIENT_ID   2
#define MQTT_CONNECT_UNAVAILABLE     3
#define MQTT_CONNECT_BAD_CREDENTIALS 4
#define MQTT_CONNECT_UNAUTHORIZED    5

#define MQTTCONNECT     1 << 4
#define MQTTCONNACK     2 << 4
#define MQTTPUBLISH     3 << 4
#define MQTTPUBACK      4 << 4
#define MQTTPUBREC      5 << 4
#define MQTTPUBREL      6 << 4
#define MQTTPUBCOMP     7 << 4
#define MQTTSUBSCRIBE   8 << 4
#define MQTTSUBACK      9 << 4
#define MQTTUNSUBSCRIBE 10 << 4
#define MQTTUNSUBACK    11 << 4
#define MQTTPINGREQ     12 << 4
#define MQTTPINGRESP    13 << 4
#define MQTTDISCONNECT  14 << 4
#define MQTTReserved    15 << 4

#define MQTTQOS0        (0 << 1)
#define MQTTQOS1        (1 << 1)
#define MQTTQOS2        (2 << 1)

#define MQTT_MAX_HEADER_SIZE 5

#if defined(ESP8266) || defined(ESP32)
#include <functional>
#define MQTT_CALLBACK_SIGNATURE std::function<void(char*, uint8_t*, unsigned int)> callback
#else
#define MQTT_CALLBACK_SIGNATURE void (*callback)(char*, uint8_t*, unsigned int)
#endif

#define CHECK_STRING_LENGTH(l,s) if (l+2+strnlen(s, this->bufferSize) > this->bufferSize) {_client->stop();return false;}

class PubSubClient : public Print {
private:
   Client* _client;
   uint8_t* buffer;
   uint32_t bufferSize;   // ← 改为 uint32_t，突破 64KB 限制
   uint16_t keepAlive;
   uint16_t socketTimeout;
   uint16_t nextMsgId;
   unsigned long lastOutActivity;
   unsigned long lastInActivity;
   bool pingOutstanding;
   MQTT_CALLBACK_SIGNATURE;
   uint32_t readPacket(uint8_t*);
   boolean readByte(uint8_t * result);
   boolean readByte(uint8_t * result, uint16_t * index);
   boolean write(uint8_t header, uint8_t* buf, uint16_t length);
   uint16_t writeString(const char* string, uint8_t* buf, uint16_t pos);
   size_t buildHeader(uint8_t header, uint8_t* buf, uint16_t length);
   IPAddress ip;
   const char* domain;
   uint16_t port;
   Stream* stream;
   int _state;
public:
   PubSubClient();
   PubSubClient(Client& client);
   PubSubClient(IPAddress, uint16_t, Client& client);
   PubSubClient(IPAddress, uint16_t, Client& client, Stream&);
   PubSubClient(IPAddress, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client);
   PubSubClient(IPAddress, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client, Stream&);
   PubSubClient(uint8_t *, uint16_t, Client& client);
   PubSubClient(uint8_t *, uint16_t, Client& client, Stream&);
   PubSubClient(uint8_t *, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client);
   PubSubClient(uint8_t *, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client, Stream&);
   PubSubClient(const char*, uint16_t, Client& client);
   PubSubClient(const char*, uint16_t, Client& client, Stream&);
   PubSubClient(const char*, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client);
   PubSubClient(const char*, uint16_t, MQTT_CALLBACK_SIGNATURE,Client& client, Stream&);

   ~PubSubClient();

   PubSubClient& setServer(IPAddress ip, uint16_t port);
   PubSubClient& setServer(uint8_t * ip, uint16_t port);
   PubSubClient& setServer(const char * domain, uint16_t port);
   PubSubClient& setCallback(MQTT_CALLBACK_SIGNATURE);
   PubSubClient& setClient(Client& client);
   PubSubClient& setStream(Stream& stream);
   PubSubClient& setKeepAlive(uint16_t keepAlive);
   PubSubClient& setSocketTimeout(uint16_t timeout);

   boolean setBufferSize(uint32_t size);   // ← uint32_t
   uint32_t getBufferSize();               // ← uint32_t

   boolean connect(const char* id);
   boolean connect(const char* id, const char* user, const char* pass);
   boolean connect(const char* id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
   boolean connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage);
   boolean connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage, boolean cleanSession);
   void disconnect();
   boolean publish(const char* topic, const char* payload);
   boolean publish(const char* topic, const char* payload, boolean retained);
   boolean publish(const char* topic, const uint8_t * payload, unsigned int plength);
   boolean publish(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained);
   boolean publish_P(const char* topic, const char* payload, boolean retained);
   boolean publish_P(const char* topic, const uint8_t * payload, unsigned int plength, boolean retained);
   boolean beginPublish(const char* topic, unsigned int plength, boolean retained);
   int endPublish();
   virtual size_t write(uint8_t);
   virtual size_t write(const uint8_t *buffer, size_t size);
   boolean subscribe(const char* topic);
   boolean subscribe(const char* topic, uint8_t qos);
   boolean unsubscribe(const char* topic);
   boolean loop();
   boolean connected();
   int state();
};

#endif
