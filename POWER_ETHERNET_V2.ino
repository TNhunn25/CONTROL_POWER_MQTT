#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <math.h>

#include "POWER_AUTO.h"
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

const char *topic_test_pub = "CRL_POWER/STATUS";
const char *topic_cmd_sub = "CRL_POWER/command";
const char *topic_ack_pub = "CRL_POWER/ACK";

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
// ===== PZEM địa chỉ Modbus =====
// Mảng lưu dữ liệu PZEM
// Bộ đệm chia sẻ để lưu chữ ký MD5 cho bản tin MQTT.
char data_MQTT[100] = {0};

// Định danh thiết bị và số lượng relay cần gửi trạng thái.
const char *device_id = "CRL_POWER_01";
const uint8_t RELAY_COUNT = 4;
// MEASUREMENT_COUNT = 1 (kênh tổng) + số relay.
const size_t MEASUREMENT_COUNT = RELAY_COUNT + 1;

//----------------
const int Auto = 30; // doc trang thai auto man
const int Man = 31;
const int Den_Auto_Man = 13;
int den = 0;
//----------------

//---------------Relay
byte Relay[] = {5, 6, 8, 11, 41, 37, 35, 33};
byte ReadRelay[] = {3, 7, 10, 12, 40, 36, 34, 32};
byte sorelay;

int vitrirelay = 0;
//-------------------

// Trạng thái xuất relay và số liệu đo mô phỏng.
int relayStates[RELAY_COUNT] = {0};
int voltages[MEASUREMENT_COUNT] = {0};
float currents[MEASUREMENT_COUNT] = {0.0f};
float energies[MEASUREMENT_COUNT] = {0.0f};
// Bộ đếm tăng dần để tạo trường Seq cho từng relay.
uint32_t sequenceCounters[RELAY_COUNT] = {0};

// Khoảng thời gian gửi dữ liệu và hệ số đổi ra giờ.
const unsigned long TELEMETRY_INTERVAL_MS = 30000UL;
const unsigned long SENSOR_POLL_INTERVAL_MS = 1000UL;
const int VOLTAGE_CHANGE_THRESHOLD = 1;       // Volt
const float CURRENT_CHANGE_THRESHOLD = 0.05f; // Ampere
const float ENERGY_CHANGE_THRESHOLD = 0.001f; // kWh

unsigned long lastTelemetryMs = 0;
unsigned long lastSensorPollMs = 0;
bool telemetryDirty = false;
uint32_t ackSequence = 0;
bool measurementDirty[MEASUREMENT_COUNT] = {false};

int lastPublishedVoltages[MEASUREMENT_COUNT] = {0};
float lastPublishedCurrents[MEASUREMENT_COUNT] = {0.0f};
float lastPublishedEnergies[MEASUREMENT_COUNT] = {0.0f};
unsigned long lastMeasurementUpdateMs[MEASUREMENT_COUNT] = {0};
bool autoModeEnabled = true;

// Khai báo trước các hàm phục vụ việc đọc và phát số liệu.
void read_pzem(uint8_t channel);
void publishMeasurements(bool publishAll);
unsigned long provideTimestamp();
void setRelayOutput(uint8_t channelIndex, bool on);
void publishRelayAck(const char *outputLabel, bool success, bool onState);

void callback(char *topic, byte *payload, unsigned int len)
{
  Serial.print("[MQTT] RX: ");
  Serial.print(topic);
  Serial.print(" -> ");
  for (unsigned int i = 0; i < len; i++)
    Serial.print((char)payload[i]);
  Serial.println();

  StaticJsonDocument<128> doc;

  DeserializationError err = deserializeJson(doc, payload, len);
  if (err)
  {
    Serial.print(F("[MQTT] JSON parse error: "));
    Serial.println(err.c_str());
    return;
  }

  // Hỗ trợ 2 dạng gói:
  //  A) {"OUTPUT":"1, ON"}           // 1..4 + ON/OFF trong cùng chuỗi
  //  B) {"OUTPUT":"1","STATE":"ON"}  // kênh và trạng thái tách riêng
  if (doc["OUTPUT"].is<const char *>())
  {
    const char *s = doc["OUTPUT"]; // "1"  hoặc "1, ON"
    int ch = atoi(s);              // lấy số kênh 1..4
    bool on = false;

    // ưu tiên đọc STATE nếu có
    if (doc["STATE"].is<const char *>())
    {
      on = (strcasecmp(doc["STATE"].as<const char *>(), "ON") == 0);
    }
    else
    {
      // nếu không có STATE, dò ngay trong chuỗi OUTPUT ("1, ON")
      on = (strstr(s, "ON") != NULL);
    }

    if (ch >= 1 && ch <= RELAY_COUNT)
    {
      setRelayOutput(static_cast<uint8_t>(ch - 1), on);
      publishRelayAck(s, true, on);
    }
    else if (strncasecmp(s, "ALL", 3) == 0)
    {
      for (uint8_t i = 0; i < RELAY_COUNT; ++i)
      {
        setRelayOutput(i, on);
      }
      publishRelayAck(s, true, on);
    }
    else
    {
      publishRelayAck(s, false, false);
    }
  }
}

// Lấy BUTTON dưới dạng số nguyên.
// ArduinoJson tự chuyển "1" (chuỗi) hoặc 1 (số) thành int.
// int btn = doc["OUTPUT"].as<int>();

// Serial.print(F("[MQTT] OUTPUT="));
// Serial.println(btn);

// if (btn == 1)
// {
//   digitalWrite(Relay[0], 0);
// }
// else if (btn == 0)
// {
//   digitalWrite(Relay[0], 1);
// }
// }

void reconnectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.println(F("MQTT connecting..."));
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password))
    {
      Serial.println(F("MQTT OK"));
      mqttClient.subscribe(topic_cmd_sub);
    }
    else
    {
      Serial.print(F("MQTT fail (state="));
      Serial.print(mqttClient.state());
      Serial.println(F(") retry..."));
      delay(2000);
    }
  }
}

//-----------------------------------------------
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

//------------------------------------------

void setup()
{
  Serial.begin(9600);
  delay(100);
  Serial.println(F("\n[SYS] Booting POWER_ETHERNET_V2"));
  Ethernet.init(53);
  UART_PZEM.begin(9600);
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  pzem.begin(); // PZEM V2 không setAddress, MUX để cô lập từng con
  Serial.println(F("PZEM-004T V2 + 74HC4052 + HC4052 lib"));
  Serial.println(F("Starting Ethernet (STATIC)..."));

  Serial.print(F("IP: "));
  Serial.println(Ethernet.localIP());

  Serial.println(F("[SYS] Setup complete, waiting for MQTT"));

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);
  mqttClient.subscribe(topic_cmd_sub, 1); // subscribe (topic, [qos])

  pinMode(Auto, INPUT_PULLUP);
  pinMode(Man, INPUT_PULLUP);
  pinMode(Den_Auto_Man, OUTPUT);

  sorelay = sizeof(Relay);
  for (int i = 0; i < sorelay; i++)
  {
    pinMode(Relay[i], OUTPUT);
    // delay(1000);
    digitalWrite(Relay[i], 1);
  }
  for (int i = 0; i < sorelay; i++)
  {
    pinMode(ReadRelay[i], INPUT);
  }
}
void loop()
{
  if (!mqttClient.connected())
    reconnectMQTT();
  mqttClient.loop();

  unsigned long now = millis();

  if (now - lastSensorPollMs >= SENSOR_POLL_INTERVAL_MS)
  {
    lastSensorPollMs = now;

    for (uint8_t i = 0; i < RELAY_COUNT; i++)
    {
      // Đọc từng kênh PZEM
      read_pzem(i);
      delay(50);
    }
  }

  bool timeToPublish = (now - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS;

  if (timeToPublish || telemetryDirty)
  {
    if (digitalRead(31) == 1)
    {
      return;
    }
    publishMeasurements(timeToPublish);
    lastTelemetryMs = now;
    // telemetryDirty = false;
  }

  Serial.println("--------------------");
  delay(1000);
}

// ===== GIỮ LẠI DUY NHẤT HÀM NÀY =====
void read_pzem(uint8_t channel)
{                                         // channel: 0..(RELAY_COUNT-1) cho 4052
  uint8_t measurementIndex = channel + 1; // 0 = tổng, 1..N = từng kênh
  if (measurementIndex >= MEASUREMENT_COUNT)
    return;

  unsigned long now = millis();

  // Chọn ngõ 4052 tương ứng (lib HC4052 dùng 0..3)
  mux.setChannel(channel);
  delay(80);
  // Đọc PZEM
  pzem_info d = pzem.getData();

  // Cập nhật buffer để publish JSON
  if (d.volt >= 0 && d.volt < 260)
    voltages[measurementIndex] = d.volt;
  if (d.ampe >= 0)
    currents[measurementIndex] = d.ampe;

  if (lastMeasurementUpdateMs[measurementIndex] != 0)
  {
    unsigned long elapsedMs = now - lastMeasurementUpdateMs[measurementIndex];
    energies[measurementIndex] += (voltages[measurementIndex] * currents[measurementIndex]) / 1000.0f * ((float)elapsedMs / 3600000.0f);
  }
  lastMeasurementUpdateMs[measurementIndex] = now;

  bool channelDirty = false;

  if (abs(voltages[measurementIndex] - lastPublishedVoltages[measurementIndex]) >= VOLTAGE_CHANGE_THRESHOLD)
  {
    channelDirty = true;
  }

  if (fabsf(currents[measurementIndex] - lastPublishedCurrents[measurementIndex]) >= CURRENT_CHANGE_THRESHOLD)
  {
    channelDirty = true;
  }

  if (fabsf(energies[measurementIndex] - lastPublishedEnergies[measurementIndex]) >= ENERGY_CHANGE_THRESHOLD)
  {
    channelDirty = true;
  }

  if (channelDirty)
  {
    // if (digitalRead(31) == 0)
    // {
    //   return;
    // }

    // đánh dấu riêng từng kênh thay đổi ====
    // Khi chỉ một output thay đổi, chỉ kênh đó được publish ngay.
    measurementDirty[measurementIndex] = true;
    telemetryDirty = true;
  }

  // (tuỳ) debug
  Serial.print("CH");
  Serial.print(channel);
  Serial.print(" | V=");
  Serial.print(voltages[measurementIndex]);
  Serial.print(" I=");
  Serial.print(currents[measurementIndex], 3);
  Serial.print(" kWh=");
  Serial.println(energies[measurementIndex], 3);
}

void publishMeasurements(bool publishAll)
{
  // Thời điểm đo dùng làm trường Time trong payload.
  unsigned long timestamp = provideTimestamp();

  for (uint8_t channel = 0; channel < RELAY_COUNT; ++channel)
  {
    uint8_t measurementIndex = channel + 1;
    if (measurementIndex >= MEASUREMENT_COUNT)
    {
      continue;
    }

    if (!publishAll && !measurementDirty[measurementIndex])
    {
      // bỏ qua kênh không thay đổi khi publish tức thì ====
      continue;
    }

    char payload[256];
    uint32_t seq = ++sequenceCounters[channel];

    // Đóng gói JSON + ký MD5; thành công thì publish lên broker.
    if (powerJsonBuildAutoPayload(device_id,
                                  measurementIndex,
                                  autoModeEnabled ? "AUTO" : "MAN",
                                  relayStates[channel] == HIGH ? 1 : 0,
                                  voltages[measurementIndex],
                                  currents[measurementIndex],
                                  energies[measurementIndex],
                                  seq,
                                  timestamp,
                                  payload,
                                  sizeof(payload)))
    {
      mqttClient.publish(topic_test_pub, (const uint8_t *)payload, strlen(payload), false);
      Serial.print("[MQTT] TX: ");
      Serial.println(payload);
      Serial.print("[MQTT] KEY: ");
      Serial.println(data_MQTT);

      lastPublishedVoltages[measurementIndex] = voltages[measurementIndex];
      lastPublishedCurrents[measurementIndex] = currents[measurementIndex];
      lastPublishedEnergies[measurementIndex] = energies[measurementIndex];
      measurementDirty[measurementIndex] = false;
    }
  }
  telemetryDirty = false;
  for (size_t i = 0; i < MEASUREMENT_COUNT; ++i)
  {
    if (measurementDirty[i])
    {
      telemetryDirty = true;
      break;
    }
  }
}

//------------------ACK

void setRelayOutput(uint8_t channelIndex, bool on)
{
  if (channelIndex >= RELAY_COUNT)
  {
    return;
  }

  int desiredState = on ? 1 : 0;
  if (relayStates[channelIndex] == desiredState)
  {
    return;
  }

  digitalWrite(Relay[channelIndex], on ? LOW : HIGH);
  relayStates[channelIndex] = desiredState;

  uint8_t measurementIndex = channelIndex + 1;
  if (measurementIndex < MEASUREMENT_COUNT)
  {
    measurementDirty[measurementIndex] = true;
  }
}

void publishRelayAck(const char *outputLabel, bool success, bool onState)
{
  StaticJsonDocument<128> ackDoc;
  ackDoc["OUTPUT"] = (outputLabel != nullptr) ? outputLabel : "";
  ackDoc["Result"] = success ? "OK" : "FAIL";
  ackDoc["Status_SYS"] = success ? (onState ? "ON" : "OFF") : "UNKNOWN";
  ackDoc["Seq"] = ++ackSequence;
  ackDoc["Time"] = provideTimestamp();

  float voltageValue = 0.0f;
  float currentValue = 0.0f;
  float energyValue = 0.0f;

  if (outputLabel != nullptr)
  {
    int channelNumber = atoi(outputLabel);
    if (channelNumber >= 1 && channelNumber <= RELAY_COUNT)
    {
      uint8_t measurementIndex = static_cast<uint8_t>(channelNumber);
      if (measurementIndex < MEASUREMENT_COUNT)
      {
        voltageValue = static_cast<float>(voltages[measurementIndex]);
        currentValue = currents[measurementIndex];
        energyValue = energies[measurementIndex];
      }
    }
  }

  char ackPayload[128];
  size_t ackLen = serializeJson(ackDoc, ackPayload, sizeof(ackPayload));
  if (ackLen > 0U)
  {
    mqttClient.publish(topic_ack_pub, ackPayload);
    Serial.print(F("[MQTT] ACK: "));
    Serial.println(ackPayload);
  }
  else
  {
    Serial.println(F("[MQTT] ACK serialization failed"));
  }
}

//-----------------------------------------

unsigned long provideTimestamp()
{
  // Với demo, sử dụng millis()/1000 làm timestamp dạng giây.
  return millis() / 100UL;
}