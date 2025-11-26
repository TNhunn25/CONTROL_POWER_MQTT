#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// Các biến mạng được định nghĩa trong POWER_ETHERNET_V2.ino
extern IPAddress ip;
extern IPAddress dns;
extern IPAddress gateway;
extern IPAddress subnet;

// Hàm apply_network_settings() đã tồn tại trong POWER_ETHERNET_V2.ino
// và chịu trách nhiệm khởi tạo lại Ethernet + MQTT với cấu hình mới.
void apply_network_settings();

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

inline void serialPrintNetworkHelp()
{
  Serial.println();
  Serial.println(F("=== Cấu hình mạng qua Serial ==="));
  Serial.println(F("Cú pháp:"));
  Serial.println(F("  SET IP <addr>"));
  Serial.println(F("  SET GATEWAY <addr>"));
  Serial.println(F("  SET SUBNET <addr>"));
  Serial.println(F("  SET DNS <addr>"));
  Serial.println(F("  SHOW            (xem cấu hình hiện tại)"));
  Serial.println(F("  HELP            (in hướng dẫn này)"));
  Serial.println();
}

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

inline bool serialHandleSetCommand(const String &field, const String &value)
{
  String upperField = field;
  upperField.toUpperCase();

  bool changed = false;
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
    Serial.print(F("[SERIAL] Tham số không hỗ trợ: "));
    Serial.println(field);
    return false;
  }

  if (changed)
  {
    apply_network_settings();
    serialPrintNetworkState();
  }

  return changed;
}

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
      Serial.println(F("[SERIAL] Thiếu tham số. Dùng: SET <IP|GATEWAY|SUBNET|DNS> <addr>"));
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
  }
  else if (command == F("HELP"))
  {
    serialPrintNetworkHelp();
  }
  else
  {
    Serial.print(F("[SERIAL] Lệnh không hỗ trợ: "));
    Serial.println(line);
    serialPrintNetworkHelp();
  }
}

inline void serialTick()
{
  static char buffer[64];
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
  serialPrintNetworkHelp();
  serialPrintNetworkState();
}
