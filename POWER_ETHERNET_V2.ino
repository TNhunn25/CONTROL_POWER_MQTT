#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

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

const char *topic_test_pub = "rd/dempw";
const char *topic_cmd_sub = "ems_prod/EMS_HG000_0000/command";
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
// ===== PZEM địa chỉ Modbus =====
// Mảng lưu dữ liệu PZEM
// Bộ đệm chia sẻ để lưu chữ ký MD5 cho bản tin MQTT.
char data_MQTT[100] = {0};

// Định danh thiết bị và số lượng relay cần gửi trạng thái.
const char *device_id = "EMS_HG000_0000";
const uint8_t RELAY_COUNT = 4;
// MEASUREMENT_COUNT = 1 (kênh tổng) + số relay.
const size_t MEASUREMENT_COUNT = RELAY_COUNT + 1;

// Trạng thái xuất relay và số liệu đo mô phỏng.
int relayStates[RELAY_COUNT] = {HIGH, HIGH, HIGH, HIGH};
int voltages[MEASUREMENT_COUNT] = {0};
float currents[MEASUREMENT_COUNT] = {0.0f};
float energies[MEASUREMENT_COUNT] = {0.0f};
// Bộ đếm tăng dần để tạo trường Seq cho từng relay.
uint32_t sequenceCounters[RELAY_COUNT] = {0};

// Khoảng thời gian gửi dữ liệu và hệ số đổi ra giờ.
const unsigned long TELEMETRY_INTERVAL_MS = 5000UL;
const float HOURS_PER_INTERVAL = (float)TELEMETRY_INTERVAL_MS / 3600000.0f;

unsigned long lastTelemetryMs = 0;
bool autoModeEnabled = true;

// Khai báo trước các hàm phục vụ việc đọc và phát số liệu.
void publishMeasurements();
unsigned long provideTimestamp();
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

// void read_pzem(uint8_t channel)
// {
//   mux.setChannel(channel); // chọn kênh (lib sẽ disable/enable ngắn)
//   delay(80);

//   pzem_info d = pzem.getData(); // đọc dữ liệu PZEM
//   Serial.print("CH");
//   Serial.print(channel);
//   Serial.print(" | V=");
//   Serial.print(d.volt);
//   Serial.print(" I =");
//   Serial.print(d.ampe);
//   Serial.print(" P=");
//   Serial.print(d.power);
//   Serial.print(" Pf =");
//   Serial.print(d.powerFactor);
//   Serial.print(" F=");
//   Serial.println(d.freq);
// }
/////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);
  Ethernet.init(53);
  UART_PZEM.begin(9600);
  mqttClient.setBufferSize(1024);
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  pzem.begin(); // PZEM V2 không setAddress, MUX để cô lập từng con
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

  unsigned long now = millis();

  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS)
  {
    lastTelemetryMs = now;

    for (uint8_t i = 0; i < RELAY_COUNT; i++)
    {
      // Đọc từng kênh PZEM (hiện đang mô phỏng dữ liệu).
      read_pzem(i);
      delay(50);
    }
    // Sau khi cập nhật mảng số liệu, tiến hành đóng gói và gửi MQTT.
    publishMeasurements();
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

  // Cộng dồn kWh theo nhịp đo (TELEMETRY_INTERVAL_MS)
  energies[measurementIndex] += (voltages[measurementIndex] * currents[measurementIndex]) / 1000.0f * ((float)TELEMETRY_INTERVAL_MS / 3600000.0f);

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

void publishMeasurements()
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
    }
  }
}

unsigned long provideTimestamp()
{
  // Với demo, sử dụng millis()/1000 làm timestamp dạng giây.
  return millis() / 1000UL;
}