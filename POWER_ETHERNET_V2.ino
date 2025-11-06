#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
// ===== Static IP Config =====
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34};
IPAddress ip(192, 168, 80, 196);
IPAddress dns(8, 8, 8, 8);
IPAddress gateway(192, 168, 80, 254);
IPAddress subnet(255, 255, 255, 0);
// ===== MQTT Info =====
const char *mqtt_server = "mqtt.dev.altasoftware.vn";
const int mqtt_port = 1883;
const char *mqtt_user = "altamedia";
const char *mqtt_password = "Altamedia@%";
const char *mqtt_client_id = "DEMO_HG_QUANTRAC_2025";

const char *topic_test_pub = "rd/dempw";
const char *topic_cmd_sub = "ems_prod/EMS_HG000_0000/command";
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
// ===== PZEM địa chỉ Modbus =====
// Mảng lưu dữ liệu PZEM
void callback(char *topic, byte *payload, unsigned int len)
{
  Serial.print("[MQTT] RX: ");
  Serial.print(topic);
  Serial.print(" -> ");
  for (unsigned int i = 0; i < len; i++)
    Serial.print((char)payload[i]);
  Serial.println();
}
void reconnectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.println("MQTT connecting...");
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password))
    {
      Serial.println("MQTT OK");
      mqttClient.subscribe(topic_cmd_sub);
    }
    else
    {
      Serial.println("MQTT fail, retry...");
      delay(2000);
    }
  }
}

///////////////////////////////////////////////////
#include "HC4052.h"              // lib multiplexer
#include "Hshopvn_Pzem004t_V2.h" // lib PZEM V2

// Khuyên dùng chân “an toàn” trên Mega (tránh D13 vì SPI LED):
#define MUX_A 28
#define MUX_B 27
// Nếu EN đã kéo GND cứng: KHÔNG truyền enablePin
HC4052 mux(MUX_A, MUX_B); // EN cứng GND

// Nếu bạn muốn điều khiển EN bằng 1 chân ví dụ D24:
// #define MUX_EN 24
// HC4052 mux(MUX_A, MUX_B, MUX_EN);  // nhớ nối EN của IC vào D24 (KHÔNG kéo GND)

#define UART_PZEM Serial3
Hshopvn_Pzem004t_V2 pzem(&UART_PZEM);
/////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);
  Ethernet.init(53);
  UART_PZEM.begin(9600);
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  pzem.begin();                      // PZEM V2 không setAddress, MUX để cô lập từng con
  Serial.println("PZEM-004T V2 + 74HC4052 + HC4052 lib");
  Serial.println("Starting Ethernet (STATIC)...");

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);
}
void loop()
{
  if (!mqttClient.connected())
    reconnectMQTT();
  mqttClient.loop();

  static unsigned long t = 0;
  if (millis() - t > 5000)
  {
    t = millis();
    mqttClient.publish(topic_test_pub, "hello_from_static_ip");
    Serial.println("Sent: hello_from_static_ip");
  }

    for (uint8_t i=0; i<4; i++) 
  { 
    read_pzem(i); delay(300); 
  }
  Serial.println("--------------------"); 
  delay(1000);
}