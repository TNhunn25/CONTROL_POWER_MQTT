#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// Các biến mạng được định nghĩa trong POWER_ETHERNET_V2.ino
extern IPAddress ip;
extern IPAddress dns;
extern IPAddress gateway;
extern IPAddress subnet;
extern bool telemetryDirty;
extern uint8_t nextPublishChannel;

// cờ telemetry trong project
extern bool telemetryDirty;
extern uint8_t nextPublishChannel;

// MQTT(SYS)
extern char mqtt_server[];
extern int mqtt_port;
extern char mqtt_user[];
extern char mqtt_password[];
extern char mqtt_client_id[];

// Hàm apply_network_settings() đã tồn tại trong POWER_ETHERNET_V2.ino
// và chịu trách nhiệm khởi tạo lại Ethernet + MQTT với cấu hình mới.
void apply_network_settings();
// Lưu cấu hình mạng hiện tại xuống EEPROM
void persistNetworkConfig();

inline void serialPrintNetworkState()
{
  Serial.println(F("[NET] Cấu hình hiện tại:"));
  Serial.print(F("  IP      : "));
  Serial.println(ip);
  Serial.print(F("  Gateway : "));
  Serial.println(gateway);
  Serial.print(F("  Subnet  : "));
  Serial.println(subnet);
  Serial.print(F("  DNS     : "));
  Serial.println(dns);
}

inline void serialPrintMqttState()
{
  Serial.println(F("[SYS] cấu hình hiện tại:"));
  Serial.print(F("  Server : "));
  Serial.println(mqtt_server);
  Serial.print(F("  Port : "));
  Serial.println(mqtt_port);
  Serial.print(F("  User : "));
  Serial.println(mqtt_user);
  Serial.print(F("  Pass : "));
  Serial.println(mqtt_password);
  Serial.print(F("  Client : "));
  Serial.println(mqtt_client_id); // chỉ hiển thị không set
}

inline void serialPrintHelp()
{
  Serial.println();
  Serial.println(F("=== Cấu hình NET + SYS qua Serial ==="));
  Serial.println(F("Cú pháp:"));
  Serial.println(F("--NET--"));
  Serial.println(F("  SET IP <addr>"));
  Serial.println(F("  SET GATEWAY <addr>"));
  Serial.println(F("  SET SUBNET <addr>"));
  Serial.println(F("  SET DNS <addr>"));
  Serial.println(F(""));
  Serial.println(F("--SYS--"));
  Serial.println(F("  SET SERVER <host>"));
  Serial.println(F("  SET PORT <port>"));
  Serial.println(F("  SET USER <user>"));
  Serial.println(F("  SET PASS <pass>"));
  Serial.println(F(""));
  Serial.println(F("  SHOW            (xem cấu hình hiện tại)"));
  Serial.println(F("  HELP            (in hướng dẫn này)"));
  Serial.println();
}

// Helper parse IP
inline bool serialParseIp(const String &text, IPAddress &out)
{
  IPAddress candidate;
  if (!candidate.fromString(text))
  {
    Serial.print(F("[SERIAL] Địa chỉ IP không hợp lệ: "));
    Serial.println(text);
    return false;
  }

  out = candidate;
  return true;
}

// Xử lý SYS (MQTT)
inline bool serialHandleSetMqttField(const String &fieldUpper, const String &value)
{
  if (fieldUpper == F("SERVER"))
  {
    if (value.length() >= 48) // phải khớp với kích thước mqtt_server[]
    {
      Serial.println(F("[SERIAL][SYS] server quá dài (max 47 ký tự)."));
      return false;
    }
    value.toCharArray(mqtt_server, 48);
  }
  else if (fieldUpper == F("PORT"))
  {
    int port = value.toInt();
    if (port <= 0 || port > 65535)
    {
      Serial.println(F("[SERIAL][SYS] port không hợp lệ."));
      return false;
    }
    mqtt_port = port;
  }
  else if (fieldUpper == F("USER"))
  {
    if (value.length() >= 32) // phải khớp với kích thước mqtt_user[]
    {
      Serial.println(F("[SERIAL][SYS] user quá dài (max 31 ký tự)."));
      return false;
    }
    value.toCharArray(mqtt_user, 32);
  }
  else if (fieldUpper == F("PASS"))
  {
    if (value.length() >= 32) // phải khớp với kích thước mqtt_password[]
    {
      Serial.println(F("[SERIAL][SYS] password quá dài (max 31 ký tự)."));
      return false;
    }
    value.toCharArray(mqtt_password, 32);
  }
  else
  {
    return false; // không phải field MQTT
  }

  // Lưu + apply lại NET + SYS
  persistNetworkConfig();
  apply_network_settings();
  telemetryDirty = true;
  nextPublishChannel = 0;

  serialPrintMqttState();
  return true;
}

// Router lệnh SET
inline bool serialHandleSetCommand(const String &field, const String &value)
{
  String upperField = field;
  upperField.toUpperCase();

  bool changed = false;

  // --- NET ---
  if (upperField == F("IP"))
  {
    changed = serialParseIp(value, ip);
  }
  else if (upperField == F("GATEWAY"))
  {
    changed = serialParseIp(value, gateway);
  }
  else if (upperField == F("SUBNET"))
  {
    changed = serialParseIp(value, subnet);
  }
  else if (upperField == F("DNS"))
  {
    changed = serialParseIp(value, dns);
  }
  else
  {
    // --- SYS (MQTT) ---
    if (!serialHandleSetMqttField(upperField, value))
    {
      Serial.print(F("[SERIAL] Tham số không hỗ trợ: "));
      Serial.println(field);
      return false;
    }
    else
    {
      return true; // đã xử lý MQTT xong
    }
  }

  if (changed)
  {
    persistNetworkConfig();
    apply_network_settings();
    telemetryDirty = true;
    nextPublishChannel = 0;
    serialPrintNetworkState();
  }

  return changed;
}

// Xử lý dòng lệnh
inline void serialProcessCommand(String line)
{
  line.trim();
  if (line.length() == 0)
  {
    return;
  }

  int spaceIndex = line.indexOf(' ');
  String command = (spaceIndex >= 0) ? line.substring(0, spaceIndex) : line;
  command.toUpperCase();
  String remainder = (spaceIndex >= 0) ? line.substring(spaceIndex + 1) : String();
  remainder.trim();

  if (command == F("SET"))
  {
    int nextSpace = remainder.indexOf(' ');
    if (nextSpace < 0)
    {
      Serial.println(F("[SERIAL] Thiếu tham số. Dùng: SET <...> <value>"));
      return;
    }

    String field = remainder.substring(0, nextSpace);
    String value = remainder.substring(nextSpace + 1);
    value.trim();

    if (!serialHandleSetCommand(field, value))
    {
      Serial.println(F("[SERIAL] Không thể cập nhật cấu hình."));
    }
  }
  else if (command == F("SHOW"))
  {
    serialPrintNetworkState();
    serialPrintMqttState();
  }
  else if (command == F("HELP"))
  {
    serialPrintHelp();
  }
  else
  {
    Serial.print(F("[SERIAL] Lệnh không hỗ trợ: "));
    Serial.println(line);
    serialPrintHelp();
  }
}

inline void serialTick()
{
  static char buffer[96];
  static size_t index = 0;

  while (Serial.available() > 0)
  {
    char c = static_cast<char>(Serial.read());
    if (c == '\r')
    {
      continue; // bỏ qua CR
    }

    if (c == '\n')
    {
      buffer[index] = '\0';
      serialProcessCommand(String(buffer));
      index = 0;
      continue;
    }

    if (index < sizeof(buffer) - 1)
    {
      buffer[index++] = c;
    }
  }
}

inline void serialInitNetworkConfig()
{
  serialPrintHelp();
  serialPrintNetworkState();
  serialPrintMqttState();
}
