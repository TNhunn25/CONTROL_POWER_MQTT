#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <math.h>
#include <EEPROM.h>
#include <Wire.h>

#include "POWER_AUTO.h"
#include "lcdv2.h"

#define LCDV2_EMBEDDED
// ===== Static IP Config =====
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34};
IPAddress ip(192, 168, 80, 5);
IPAddress dns(8, 8, 8, 8);
IPAddress gateway(192, 168, 80, 254);
IPAddress subnet(255, 255, 255, 0);
// ===== MQTT Info =====
const char *mqtt_server = "mqtt.dev.altasoftware.vn";
const int mqtt_port = 1883;
const char *mqtt_user = "altamedia";
const char *mqtt_password = "Altamedia@%";
const char *mqtt_client_id = "DEMO_CONTROL_POWER_2025";

const char *topic_test_pub = "CRL_POWER/STATUS";
const char *topic_cmd_sub = "CRL_POWER/command";
const char *topic_ack_pub = "CRL_POWER/ACK";
const char *topic_info = "CRL_POWER/INFO";

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);
// ===== PZEM địa chỉ Modbus =====
// Mảng lưu dữ liệu PZEM
// Bộ đệm chia sẻ để lưu chữ ký MD5 cho bản tin MQTT.
char data_MQTT[100] = {0};

// Định danh thiết bị và số lượng relay cần gửi trạng thái.
const char *device_id = "CONTROL_POWER";
// const uint8_t RELAY_COUNT = 4;
// MEASUREMENT_COUNT = 1 (kênh tổng) + số relay.
const size_t MEASUREMENT_COUNT = RELAY_COUNT + 1;

// Địa chỉ lưu trạng thái relay trong EEPROM.
const uint8_t EEPROM_SIGNATURE = 0xA5;
const int EEPROM_SIGNATURE_ADDR = 0;
const int EEPROM_RELAY_BASE_ADDR = EEPROM_SIGNATURE_ADDR + 1;
const int EEPROM_ENERGY_BASE_ADDR = EEPROM_RELAY_BASE_ADDR + RELAY_COUNT;
const int EEPROM_ENERGY_SIGNATURE_ADDR = EEPROM_ENERGY_BASE_ADDR + (MEASUREMENT_COUNT * sizeof(float));
const uint8_t EEPROM_ENERGY_SIGNATURE = 0x5A;
const float NGUONG_LUU_NANG_LUONG = 0.01f; // kWh

// //----------------
// const int Auto = 30; // doc trang thai auto man
// const int Man = 31;
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
// int relayStates[RELAY_COUNT] = {0};
int voltages[MEASUREMENT_COUNT] = {0};
float currents[MEASUREMENT_COUNT] = {0.0f};
float energies[MEASUREMENT_COUNT] = {0.0f};
float nangLuongDaLuu[MEASUREMENT_COUNT] = {0.0f};

// Bộ đếm tăng dần để tạo trường Seq cho từng relay.
uint32_t sequenceCounters[RELAY_COUNT] = {0};

// Khoảng thời gian gửi dữ liệu và hệ số đổi ra giờ.
const unsigned long TELEMETRY_INTERVAL_MS = 30000UL;
const unsigned long SENSOR_POLL_INTERVAL_MS = 1000UL;
// thời gian chờ giữa các relay khi bật tuần tự (ms)
const unsigned long TIME_CHO_MOI_KENH_MS = 3000UL;

const int VOLTAGE_CHANGE_THRESHOLD = 1;       // Volt
const float CURRENT_CHANGE_THRESHOLD = 0.05f; // Ampere
const float ENERGY_CHANGE_THRESHOLD = 0.001f; // kWh

// unsigned long lastTelemetryMs = 0;
unsigned long lastSensorPollMs = 0;
bool telemetryDirty = false;
uint32_t ackSequence = 0;
bool measurementDirty[MEASUREMENT_COUNT] = {false};

// Duy trì vị trí kênh sẽ publish tiếp theo để đảm bảo gửi tuần tự.
uint8_t nextPublishChannel = 0;

int lastPublishedVoltages[MEASUREMENT_COUNT] = {0};
float lastPublishedCurrents[MEASUREMENT_COUNT] = {0.0f};
float lastPublishedEnergies[MEASUREMENT_COUNT] = {0.0f};
unsigned long lastMeasurementUpdateMs[MEASUREMENT_COUNT] = {0};
// bool autoModeEnabled = true;
// bool autoModeEnabled;

unsigned long setupTime = 0;

// Khai báo trước các hàm phục vụ việc đọc và phát số liệu.
void read_pzem(uint8_t channel);
void publishMeasurements(bool publishAll);
unsigned long provideTimestamp();
void setRelayOutput(uint8_t channelIndex, bool on);
void publishRelayAck(const char *outputLabel, bool success, bool onState);
void loadRelayStates();
void docNangLuongTuEEPROM();
void luuNangLuongVaoEEPROM(uint8_t chiSo, float giaTriMoi);
void dongBoTrangThaiRelayTuPhanCung();
void batRelayTheoThuTu(); // MAN -> AUTO: bật lại relay theo thứ tự, dựa trên trạng thái đang lưu trong relayStates[]

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

    // ===== KHÓA LỆNH KHI ĐANG MAN =====
    if (!autoModeEnabled)
    { // đang ở MAN -> không cho điều khiển từ xa
      bool cur = false;
      if (ch >= 1 && ch <= RELAY_COUNT)
        cur = (relayStates[ch - 1] == 1);
      publishRelayAck(s, false, cur); // báo FAIL, giữ nguyên trạng thái hiện tại
      return;                         // THOÁT SỚM, KHÔNG setRelayOutput
    }
    // ==================================

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
        if (i + 1U < RELAY_COUNT)
        {
          delay(TIME_CHO_MOI_KENH_MS); // 3s
        }
      }
      publishRelayAck(s, true, on);
    }
    else
    {
      publishRelayAck(s, false, false);
    }
  }
}

void reconnectMQTT()
{
  // Thời gian giữa 2 lần thử reconnect
  const unsigned long RECONNECT_INTERVAL_MS = 5000;
  static unsigned long lastReconnectAttempt = 0;

  // Nếu đang connected rồi thì thôi
  if (mqttClient.connected())
    return;

  unsigned long now = millis();

  // Chưa tới lúc thử lại thì return, tránh block loop()
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL_MS)
    return;

  lastReconnectAttempt = now;

  // Nếu lib Ethernet hỗ trợ check link, dùng luôn (nếu không có macro này thì bỏ #ifdef/#endif cũng được)
#ifdef ETHERNET_HAS_READLINKSTATUS
  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println(F("Ethernet link DOWN, skip MQTT reconnect"));
    return;
  }
#endif

  Serial.println(F("MQTT connecting..."));

  char willPayload[128];
  snprintf(willPayload, sizeof(willPayload),
           "{\"id\":\"%s\",\"status\":\"offline\"}", device_id);

  if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password, topic_info, 1, 1, willPayload))
  {
    Serial.println(F("MQTT OK"));
    mqttClient.subscribe(topic_cmd_sub);
    clearRetainedCmd();

    char onlinePayload[128];
    snprintf(onlinePayload, sizeof(onlinePayload),
             "{\"id\":\"%s\",\"status\":\"online\"}", device_id);
    mqttClient.publish(topic_info, onlinePayload, true);
  }
  else
  {
    Serial.print(F("MQTT fail (state="));
    Serial.print(mqttClient.state());
    Serial.println(F(") will retry later..."));
    // KHÔNG delay ở đây nữa, trả quyền lại cho loop()
  }
}

//-----------------------------------------------
#include "HC4052.h"              // lib multiplexer
#include "Hshopvn_Pzem004t_V2.h" // lib PZEM V2

// Khuyên dùng chân “an toàn” trên Mega (tránh D13 vì SPI LED):
#ifndef MUX_A
#define MUX_A 28
#endif
#ifndef MUX_B
#define MUX_B 27
#endif
// Nếu EN đã kéo GND cứng: KHÔNG truyền enablePin (instance mux nằm trong lcdv2.h)

#ifndef UART_PZEM
#define UART_PZEM Serial3
#endif

#ifndef UART_PZEM_TONG
#define UART_PZEM_TONG Serial2
#endif
Hshopvn_Pzem004t_V2 pzemTong(&UART_PZEM_TONG);

void read_pzem_tong()
{
  float sumA = 0.0f, sumKwh = 0.0f;
  int vmax = 0;
  for (uint8_t i = 1; i <= RELAY_COUNT; ++i)
  {
    sumA += currents[i];
    sumKwh += energies[i];
    if (voltages[i] > vmax)
      vmax = voltages[i];
  }

  // Tính tổng mới
  int v0_new = vmax;
  float i0_new = sumA;
  float e0_new = sumKwh;

  // Cập nhật bộ đệm
  voltages[0] = v0_new;
  currents[0] = i0_new;
  energies[0] = e0_new;

  // CHỈ đánh dấu dirty khi thay đổi vượt ngưỡng
  bool totalChanged =
      (abs(v0_new - lastPublishedVoltages[0]) >= VOLTAGE_CHANGE_THRESHOLD) ||
      (fabsf(i0_new - lastPublishedCurrents[0]) >= CURRENT_CHANGE_THRESHOLD) ||
      (fabsf(e0_new - lastPublishedEnergies[0]) >= ENERGY_CHANGE_THRESHOLD);

  if (totalChanged)
  {
    measurementDirty[0] = true;
    telemetryDirty = true;
  }

  // dữ liệu cho LCD
  if ((sizeof(ch) / sizeof(ch[0])) > 0)
  {
    ch[0].V = v0_new;
    ch[0].I = i0_new;
    ch[0].P = e0_new; // kWh
  }
}

//------------------------------------------

// Gọi sau khi subscribe để xóa mọi lệnh còn retain
static void clearRetainedCmd()
{
  // PubSubClient overload: publish(topic, payload, length, retained)
  mqttClient.publish(topic_cmd_sub, (const uint8_t *)"", 0, true);
}

void setup()
{
  Serial.begin(9600);
  delay(100);
  lcdv2_begin();
  lcd.setCursor(0, 0);
  setupTime = millis();
  Serial.println(F("\n[SYS] Booting POWER_ETHERNET_V2"));
  Ethernet.init(53);
  UART_PZEM.begin(9600);

  UART_PZEM_TONG.begin(9600);
  pzemTong.begin();
  pzemTong.setTimeout(100);

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
  clearRetainedCmd();

  pinMode(Auto, INPUT_PULLUP);
  pinMode(Man, INPUT_PULLUP);

  // pinMode(Auto, INPUT); // công tắc cấp Vcc khi chọn
  // pinMode(Man, INPUT);  // MAN = HIGH
  pinMode(Den_Auto_Man, OUTPUT);

  //----------------------------
  bool manActive = (digitalRead(Man) == HIGH);   // chân 31
  bool autoActive = (digitalRead(Auto) == HIGH); // chân 30

  if (manActive && !autoActive)
    autoModeEnabled = false; // MAN
  else if (autoActive && !manActive)
    autoModeEnabled = true; // AUTO
  else
    autoModeEnabled = true; // mơ hồ -> mặc định AUTO

  digitalWrite(Den_Auto_Man, autoModeEnabled ? LOW : HIGH); // LED bật khi MAN
  for (uint8_t i = 1; i < MEASUREMENT_COUNT; ++i)
    measurementDirty[i] = true;
  telemetryDirty = true;
  nextPublishChannel = 0;
  //----------------------------

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
  loadRelayStates();

  // Khi khởi động ở chế độ AUTO, bật lại relay theo thứ tự tránh kích đồng thời
  if (autoModeEnabled)
  {
    batRelayTheoThuTu();
  }

  docNangLuongTuEEPROM();
  dongBoTrangThaiRelayTuPhanCung();
}

// Vòng lặp chính
void loop()
{
  unsigned long now = millis();
  if (!mqttClient.connected())
  {
    reconnectMQTT();
  }
  else
  {
    mqttClient.loop();
  }

  if (millis() - setupTime > 5000)
  {
    lcdv2_tick_display();
  }

  if (now - lastSensorPollMs >= SENSOR_POLL_INTERVAL_MS)
  {
    lastSensorPollMs = now;

    if (timeout_readpzem <= millis())
    {
      for (uint8_t i = 0; i < RELAY_COUNT; i++)
      {
        // Đọc từng kênh PZEM
        read_pzem(i);
      }
    }
    read_pzem_tong();
  }

  //------------------------------------
  static bool lastAuto = autoModeEnabled;

  bool manActive = (digitalRead(Man) == HIGH);
  bool autoActive = (digitalRead(Auto) == HIGH);

  // Xác định chế độ hiện tại từ 2 input MAN/AUTO
  bool curAuto = lastAuto;
  if (manActive && !autoActive)
  {
    curAuto = false; // MAN
  }
  else if (autoActive && !manActive)
  {
    curAuto = true; // AUTO
  }

  // Nếu có thay đổi chế độ
  if (curAuto != lastAuto)
  {
    autoModeEnabled = curAuto;
    digitalWrite(Den_Auto_Man, curAuto ? LOW : HIGH); // LED bật khi MAN

    // AUTO -> MAN: lưu nguyên trạng thái hiện tại của các kênh xuống EEPROM
    if (!curAuto)
    {
      for (uint8_t i = 0; i < RELAY_COUNT; ++i)
      {
        EEPROM.update(EEPROM_RELAY_BASE_ADDR + i, (uint8_t)relayStates[i]);
        uint8_t measurementIndex = i + 1;
        if (measurementIndex < MEASUREMENT_COUNT)
          measurementDirty[measurementIndex] = true;
      }
      telemetryDirty = true;
    }
    else
    {
      batRelayTheoThuTu(); // Man -> auto bật lại relay theo thứ tự, tránh khởi động đồng thời
    }

    // Cập nhật lại telemetry
    for (uint8_t i = 1; i < MEASUREMENT_COUNT; ++i)
      measurementDirty[i] = true;
    telemetryDirty = true;
    nextPublishChannel = 0;
    lastAuto = curAuto;
  }

  // Khi dang ở Man: đọc trạng thái thực tế từ công tắc để sync vào mạch
  if (!autoModeEnabled)
  {
    dongBoTrangThaiRelayTuPhanCung();
  }
  //------------------------------------

  bool timeToPublish = (now - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS;

  if (timeToPublish || telemetryDirty)
  {
    publishMeasurements(timeToPublish);
    if (timeToPublish)
    {
      lastTelemetryMs = now;
    }
  }
}

// Thêm 2 macro ở đầu file (hoặc ngay trên read_pzem):
#define V_MIN_ENERGY 90    // chỉ tính kWh khi V ≥ 90V
#define I_MIN_ENERGY 0.03f // và I ≥ 0.03A (tùy phần cứng)

void read_pzem(uint8_t channel)
{
  uint8_t measurementIndex = channel + 1;
  if (measurementIndex >= MEASUREMENT_COUNT)
    return;

  unsigned long now = millis();

  mux.setChannel(channel);
  delay(80);
  pzem_info d = pzem.getData();

  if (d.volt >= 0 && d.volt < 260)
    voltages[measurementIndex] = d.volt;
  if (d.ampe >= 0)
    currents[measurementIndex] = d.ampe;

  if (lastMeasurementUpdateMs[measurementIndex] != 0)
  {
    unsigned long elapsedMs = now - lastMeasurementUpdateMs[measurementIndex];

    // === chống nhiễu tích phân năng lượng
    float vEff = (voltages[measurementIndex] >= V_MIN_ENERGY) ? (float)voltages[measurementIndex] : 0.0f;
    float iEff = (currents[measurementIndex] >= I_MIN_ENERGY) ? currents[measurementIndex] : 0.0f;

    // Wh = V*A*h ; kWh = Wh/1000
    energies[measurementIndex] += (vEff * iEff) / 1000.0f * ((float)elapsedMs / 3600000.0f);
    if (!isfinite(energies[measurementIndex]) || energies[measurementIndex] < 0.0f)
      energies[measurementIndex] = 0.0f;
  }
  lastMeasurementUpdateMs[measurementIndex] = now;

  // === dữ liệu cho LCD: V, I, kWh (round 3 số để không rung)

  if (channel < (sizeof(ch) / sizeof(ch[0])))
  {
    uint8_t displayIndex = channel + 1; // ch[0] dành cho kênh tổng
    if (displayIndex < CHANNEL_COUNT)
    {
      ch[displayIndex].V = voltages[measurementIndex];
      ch[displayIndex].I = currents[measurementIndex];

      float kwh_disp = energies[measurementIndex];
      kwh_disp = floorf(kwh_disp * 1000.0f + 0.5f) / 1000.0f; // round về 0.001 kWh
      ch[displayIndex].P = kwh_disp;                          // LCD sẽ in kWh từ ch[displayIndex].P
    }
  }

  luuNangLuongVaoEEPROM(measurementIndex, energies[measurementIndex]);

  bool channelDirty = false;
  if (abs(voltages[measurementIndex] - lastPublishedVoltages[measurementIndex]) >= VOLTAGE_CHANGE_THRESHOLD)
    channelDirty = true;
  if (fabsf(currents[measurementIndex] - lastPublishedCurrents[measurementIndex]) >= CURRENT_CHANGE_THRESHOLD)
    channelDirty = true;
  if (fabsf(energies[measurementIndex] - lastPublishedEnergies[measurementIndex]) >= ENERGY_CHANGE_THRESHOLD)
    channelDirty = true;

  if (channelDirty)
  {
    measurementDirty[measurementIndex] = true;
    telemetryDirty = true;
  }

  // debug
  // Serial.print("CH");
  // Serial.print(channel);
  // Serial.print(" | V=");
  // Serial.print(voltages[measurementIndex]);
  // Serial.print(" I=");
  // Serial.print(currents[measurementIndex], 3);
  // Serial.print(" kWh=");
  // Serial.println(energies[measurementIndex], 3);
}

// Dùng để đẩy gói tin lên MQTT
void publishMeasurements(bool publishAll)
{
  if (publishAll)
  {
    // Đánh dấu tất cả kênh 0..MEASUREMENT_COUNT-1 là bẩn
    for (uint8_t i = 0; i < MEASUREMENT_COUNT; ++i)
    {
      measurementDirty[i] = true;
    }
    telemetryDirty = true;
    nextPublishChannel = 0;
  }

  unsigned long timestamp = provideTimestamp(); // thời điểm đo dùng cho payload

  // ===== PUBLISH CÁC KÊNH 1..RELAY_COUNT TRƯỚC =====
  uint8_t startChannel = nextPublishChannel;
  bool publishedAny = false;
  uint8_t lastPublishedChannel = nextPublishChannel;

  for (uint8_t attempt = 0; attempt < RELAY_COUNT; ++attempt)
  {
    uint8_t channel = (startChannel + attempt) % RELAY_COUNT; // 0..RELAY_COUNT-1
    uint8_t measurementIndex = channel + 1;                   // 1..MEASUREMENT_COUNT-1
    if (measurementIndex >= MEASUREMENT_COUNT)
      continue;
    if (!measurementDirty[measurementIndex])
      continue;

    char payload[256];
    uint32_t seq = ++sequenceCounters[channel];

    // Đóng gói JSON + ký MD5; thành công thì publish lên broker.
    if (powerJsonBuildAutoPayload(device_id,
                                  measurementIndex,
                                  autoModeEnabled ? "AUTO" : "MAN",
                                  // relayStates[channel] == HIGH ? 1 : 0,
                                  relayStates[channel] ? 1 : 0,
                                  voltages[measurementIndex],
                                  currents[measurementIndex],
                                  energies[measurementIndex],
                                  seq,
                                  timestamp,
                                  payload,
                                  sizeof(payload)))
    {
      if (ui32_timeout_mqtt <= millis())
      {
        mqttClient.publish(topic_test_pub, (const uint8_t *)payload, strlen(payload), false);
        // Serial.print("[MQTT] TX: ");
        // Serial.println(payload);
        // Serial.print("[MQTT] KEY: ");
        // Serial.println(data_MQTT);
      }

      lastPublishedVoltages[measurementIndex] = voltages[measurementIndex];
      lastPublishedCurrents[measurementIndex] = currents[measurementIndex];
      lastPublishedEnergies[measurementIndex] = energies[measurementIndex];
      measurementDirty[measurementIndex] = false;

      publishedAny = true;
      lastPublishedChannel = channel;
    }
  }

  if (publishedAny)
  {
    nextPublishChannel = (lastPublishedChannel + 1) % RELAY_COUNT;
  }

  // ===== SAU KHI 4 KÊNH TRÊN GỬI XONG MỚI GỬI KÊNH TỔNG (index = 0) =====
  static uint32_t seq_total = 0;
  if (measurementDirty[0])
  {
    // Status tổng: 1 nếu có ít nhất một kênh con đang ON (giữ nguyên logic cũ)
    int totalStatus = 0;
    for (uint8_t i = 0; i < RELAY_COUNT; ++i)
    {
      if (relayStates[i])
      {
        totalStatus = 1;
        break;
      }
    }

    char payload[256];
    if (powerJsonBuildAutoPayload(device_id,
                                  0, // Channel Tổng
                                  autoModeEnabled ? "AUTO" : "MAN",
                                  totalStatus, // nếu muốn bỏ Status, truyền -1
                                  voltages[0],
                                  currents[0],
                                  energies[0],
                                  ++seq_total,
                                  timestamp,
                                  payload,
                                  sizeof(payload)))
    {

      // --- Đổi Channel 0 -> "Tong"---
      if (ui32_timeout_mqtt <= millis())
      {
        mqttClient.publish(topic_test_pub, (const uint8_t *)payload, strlen(payload), false);
        Serial.print("[MQTT] TX: ");
        Serial.println(payload);
        Serial.print("[MQTT] KEY: ");
        Serial.println(data_MQTT);
      }

      lastPublishedVoltages[0] = voltages[0];
      lastPublishedCurrents[0] = currents[0];
      lastPublishedEnergies[0] = energies[0];
      measurementDirty[0] = false;
    }
  }
  // ===== HẾT PHẦN TỔNG =====

  // Cập nhật cờ telemetryDirty dựa trên việc còn kênh nào bẩn hay không
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
  if (relayStates[channelIndex] != desiredState) //== sửa thành !=
  {
    digitalWrite(Relay[channelIndex], on ? LOW : HIGH);
    relayStates[channelIndex] = desiredState;
    EEPROM.update(EEPROM_RELAY_BASE_ADDR + channelIndex, static_cast<uint8_t>(desiredState));
    // return;

    uint8_t measurementIndex = channelIndex + 1;
    if (measurementIndex < MEASUREMENT_COUNT)
    {
      measurementDirty[measurementIndex] = true;
    }
    telemetryDirty = true;
  }
}

void publishRelayAck(const char *outputLabel, bool success, bool onState)
{
  StaticJsonDocument<128> ackDoc;
  char label[32];
  strncpy(label, outputLabel ? outputLabel : "", sizeof(label) - 1);
  label[sizeof(label) - 1] = '\0';
  char *p = strchr(label, ','); // cắt ", ON/OFF"
  if (p)
  {
    *p = '\0';
  }

  ackDoc["Channel"] = label;
  ackDoc["Result"] = success ? "OK" : "FAIL";
  ackDoc["Status"] = success ? (onState ? "ON" : "OFF") : "UNKNOWN";
  ackDoc["Mode"] = autoModeEnabled ? "AUTO" : "MAN";
  ackDoc["Seq"] = ++ackSequence;
  ackDoc["Time"] = provideTimestamp();

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
  return millis() / 200UL;
}

void loadRelayStates()
{
  bool signatureValid = (EEPROM.read(EEPROM_SIGNATURE_ADDR) == EEPROM_SIGNATURE);
  if (!signatureValid)
  {
    EEPROM.write(EEPROM_SIGNATURE_ADDR, EEPROM_SIGNATURE);
    for (uint8_t i = 0; i < RELAY_COUNT; ++i)
    {
      EEPROM.write(EEPROM_RELAY_BASE_ADDR + i, static_cast<uint8_t>(0));
    }
  }

  for (uint8_t i = 0; i < RELAY_COUNT; ++i)
  {
    uint8_t storedState = signatureValid ? EEPROM.read(EEPROM_RELAY_BASE_ADDR + i) : 0;
    storedState = (storedState == 1) ? 1 : 0;

    relayStates[i] = storedState;

    uint8_t measurementIndex = i + 1;
    if (measurementIndex < MEASUREMENT_COUNT)
    {
      measurementDirty[measurementIndex] = true;
    }
  }

  telemetryDirty = true;
}

void dongBoTrangThaiRelayTuPhanCung()
{
  // Đọc trạng thái thực tế của relay (MAN) và cập nhật biến phần mềm cho đúng.
  bool anyChanged = false;

  for (uint8_t i = 0; i < RELAY_COUNT; ++i)
  {
    int actualState = (digitalRead(ReadRelay[i]) == LOW) ? 0 : 1;

    if (relayStates[i] != actualState)
    {
      // CẬP NHẬT BỘ NHỚ CHẠY
      relayStates[i] = actualState;

      // GHI EEPROM NGAY ĐỂ LƯU "CHẾ ĐỘ CUỐI CÙNG" (kể cả MAN)
      EEPROM.update(EEPROM_RELAY_BASE_ADDR + i, static_cast<uint8_t>(actualState));

      // Đánh dấu kênh thay đổi để publish
      uint8_t measurementIndex = i + 1;
      if (measurementIndex < MEASUREMENT_COUNT)
      {
        measurementDirty[measurementIndex] = true;
      }
      anyChanged = true;
    }
  }

  if (anyChanged)
  {
    telemetryDirty = true;
  }
}

void docNangLuongTuEEPROM()
{
  bool duLieuHopLe = (EEPROM.read(EEPROM_ENERGY_SIGNATURE_ADDR) == EEPROM_ENERGY_SIGNATURE);

  for (uint8_t i = 0; i < MEASUREMENT_COUNT; ++i)
  {
    float giaTri = 0.0f;
    int diaChi = EEPROM_ENERGY_BASE_ADDR + (i * sizeof(float));

    if (duLieuHopLe)
    {
      EEPROM.get(diaChi, giaTri);
      if (!isfinite(giaTri) || giaTri < 0.0f)
      {
        giaTri = 0.0f;
      }
    }
    else
    {
      EEPROM.put(diaChi, 0.0f);
    }

    energies[i] = giaTri;
    nangLuongDaLuu[i] = giaTri;
    measurementDirty[i] = true;
  }

  if (!duLieuHopLe)
  {
    EEPROM.update(EEPROM_ENERGY_SIGNATURE_ADDR, EEPROM_ENERGY_SIGNATURE);
  }

  telemetryDirty = true;
}

void luuNangLuongVaoEEPROM(uint8_t chiSo, float giaTriMoi)
{
  if (chiSo >= MEASUREMENT_COUNT || !isfinite(giaTriMoi) || giaTriMoi < 0.0f)
  {
    return;
  }

  if (fabsf(giaTriMoi - nangLuongDaLuu[chiSo]) < NGUONG_LUU_NANG_LUONG)
  {
    return;
  }

  EEPROM.put(EEPROM_ENERGY_BASE_ADDR + (chiSo * sizeof(float)), giaTriMoi);
  nangLuongDaLuu[chiSo] = giaTriMoi;
}

// Khi chuyển từ MAN sang AUTO:
// - Dựa trên mảng relayStates[] (đã được cập nhật trong MAN)
// - Tắt hết relay trên MCU
// - Sau đó bật lại từng kênh có trạng thái 1 theo thứ tự với độ trễ cố định
void batRelayTheoThuTu()
{
  // 1) Tắt tất cả relay trên MCU trước (đảm bảo không bật đồng thời)
  for (uint8_t i = 0; i < RELAY_COUNT; ++i)
  {
    digitalWrite(Relay[i], HIGH); // HIGH = OFF (theo logic hiện tại của bạn)
  }

  // 2) Bật lại từng kênh đang được đánh dấu ON trong relayStates[]
  for (uint8_t i = 0; i < RELAY_COUNT; ++i)
  {
    if (relayStates[i])
    {
      digitalWrite(Relay[i], LOW); // LOW = ON

      // Nếu chưa phải kênh cuối, chờ trước khi bật kênh tiếp theo
      if (i + 1U < RELAY_COUNT)
      {
        delay(TIME_CHO_MOI_KENH_MS);
      }
    }
  }
}
