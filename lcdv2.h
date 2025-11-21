#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Ethernet.h>
#include "HC4052.h"
#include "Hshopvn_Pzem004t_V2.h"
#include <KY040.h>

extern IPAddress ip;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress dns;
extern mptt_port;
extern const char *mqtt_server;
extern const int mqtt_port;

LiquidCrystal_I2C lcd(0x27, 16, 2);
int currentStateCLK;
int lastStateCLK;
String currentDir = "";
unsigned long lastButtonPress = 0;

#define CLK_PIN 47 // aka. A
#define DT_PIN 48  // aka. B
KY040 g_rotaryEncoder(CLK_PIN, DT_PIN);

unsigned long lastTelemetryMs = 0;
int encoderPos = 0;
int flag_encoderPos = 0;
int pinALast, aVal;
bool bCW;
unsigned long lastReadMs = 0;
#define MUX_A 28
#define MUX_B 27
HC4052 mux(MUX_A, MUX_B);
unsigned long ui32_timeout_mqtt;
static uint8_t lastA = HIGH;
static bool validating = false;
static unsigned long t0 = 0;
static uint8_t a_locked = HIGH; // giá trị A tại thời điểm bắt đầu kiểm tra
static uint8_t b_relation = 0;  // 0: (B==A), 1: (B!=A)
unsigned long timeout_readpzem;
const unsigned long HOLD_MS = 3; // thời gian phải duy trì điều kiện (3–8 ms tuỳ nhiễu)
const int Auto = 30;             // Doc trang thai Auto Man
const int Man = 31;
const uint8_t RELAY_COUNT = 4;
const uint8_t CHANNEL_COUNT = 5;        // tổng cộng 5 kênh tính cả kênh 0 (Tatal CH)
const uint8_t DISPLAY_SCREEN_COUNT = 6; // 5 kênh đo + 1 trang trạng thái relay
bool autoModeEnabled;
int lastRelayStates[RELAY_COUNT] = {-1};
unsigned long lastDisplayUpdateMs = 0;
int relayStates[RELAY_COUNT] = {0};

#define UART_PZEM Serial3
Hshopvn_Pzem004t_V2 pzem(&UART_PZEM);

//---------------------------
// UI state machine
enum UiMode
{
  UI_MODE_VIEW = 0,     // xem số liệu (như hiện tại)
  UI_MODE_MENU,         // menu chính
  UI_MODE_NET_MENU,     // Menu cấu hình IP
  UI_MODE_NET_IP_CONFIG // Chỉnh từng actet IP
};

UiMode uiMode = UI_MODE_VIEW;

// Menu chính:
// 0: Xem số liệu (view)
// 1: Chế độ xem cấu hình IP hiện tại
// 2: Cấu hình từng IP tùy chỉnh
// 3: Thoát exit
int menuIndex = 0;
const int MAIN_MENU_ITEMS = 4;
int netFieldIndex = 0;
const int NET_MENU_ITEMS = 5; // IP, Gateway, Subnet, DNS, MQTT

// Pin nút nhấn encoder
#define SW_PIN 49

const unsigned long BUTTON_DEBOUNCE_MS = 80;

String chuString[65] = {" ", "a", "A", "b", "B", "c", "C", "d", "D", "e", "E", "f", "F", "g", "G", "h", "H", "i", "I", "j", "J", "k", "K", "l", "L", "m", "M", "n", "N", "o", "O", "p", "P", "q", "Q", "r", "R", "s", "S", "t", "T", "u", "U", "v", "V", "w", "W", ",x", "X", "y", "Y", "z", "Z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "-"};
char chuChar[65] = " aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789.-";
byte Name[5];     //={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,};
byte Nametemp[5]; //={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,};
int setchu = 0;

byte ServerAddress[32];
byte ServerAddresstemp[32];

byte UserName[32];     //={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,};
byte UserNametemp[32]; //={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,};

char temp[16];
int giatriEncoder = 0;
int Setup_1 = 0;
int Setup_2 = 0;
int Setup_3 = 0;
int Setup_4 = 0;
int nhanSetup = 0;
unsigned long timer_thoatSetup = 0;

long timer_chopchu = 0;
int dongchop = 0;
int chopchu = 0;

int ngonngu = 0;
int IPtinh = 0;
int setIPtinh = 0;
int setgiatriIP = 0;

void lcdv2_handle_button();
void lcdv2_show_main_menu();
void lcdv2_show_net_menu();
void lcdv2_show_net_ip_config();
void lcdv2_print_line(uint8_t row, const char *text);
void lcdv2_format_ip(const IPAddress &addr, char *buffer, size_t length);
//---------------------------
typedef struct
{
  float V, I, P;
} PZEMVAL;

PZEMVAL ch[CHANNEL_COUNT]; // lưu dữ liệu cho 4 kênh, cũ ch[4];
PZEMVAL lastDisplayValues[CHANNEL_COUNT];
bool lastDisplayValid[CHANNEL_COUNT] = {false, false, false, false, false};
void read_pzem_channel(int channel, PZEMVAL &out)
{
  mux.setChannel(channel);
  delay(40); // cho PZEM ổn định qua mux
  pzem_info d = pzem.getData();
  out.V = d.volt;
  out.I = d.ampe;
  out.P = d.power;
}

// Hiển thị 1 kênh lên LCD
int last_channel = -1;
bool relayStatusChanged()
{
  // Kiem tra xem trang thai tung relay co thay doi khong?
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
  {
    if (lastRelayStates[i] != relayStates[i])
    {
      return true;
    }
  }
  return false;
}

// Ham cap nhat trang thai relay duoc luu
void updateLastRelayStatus()
{
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
  {
    lastRelayStates[i] = relayStates[i];
  }
}

void lcd_show(int channel) // Anh Danh viet
{

  bool channelChanged = last_channel != channel;

  if (lastDisplayValid[channel] &&
      !channelChanged &&
      lastDisplayValues[channel].V == ch[channel].V &&
      lastDisplayValues[channel].I == ch[channel].I &&
      lastDisplayValues[channel].P == ch[channel].P)
  {
    return;
  }

  if (channelChanged)
  {
    lcd.clear();
  }

  static const char *CHANNEL_LABELS[CHANNEL_COUNT] = {
      "Total", "CH1", "CH2", "CH3", "CH4"};

  lcd.setCursor(0, 0);
  lcd.print(CHANNEL_LABELS[channel]);
  lcd.print(":");

  lcd.print(ch[channel].V, 0);
  lcd.print("V ");

  lcd.print(autoModeEnabled ? "AUTO" : "MAN");

  lcd.setCursor(0, 1);
  lcd.print(ch[channel].I, 1);
  lcd.print("A ");

  lcd.print(ch[channel].P, 2);
  lcd.print("Kwh");

  lcd.print("   ");

  last_channel = channel;
  lastDisplayValues[channel] = ch[channel];
  lastDisplayValid[channel] = true;
}

void lcdshowchannelstate(int channel)
{
  if (relayStatusChanged() || last_channel != channel)
  {
    // lcd.clear();
    for (uint8_t i = 0; i < RELAY_COUNT; i++)
    {
      if (i == 0)
      {
        lcd.setCursor(0, 0);
      }
      else if (i == 2)
      {
        lcd.setCursor(0, 1);
      }
      else
      {
      }
      lcd.print("CH");
      lcd.print(i + 1);
      lcd.print(":");

      if (relayStates[i] == 1)

        lcd.print("ON");

      else
        lcd.print("OFF");

      if (i < RELAY_COUNT - 1)
        lcd.print(" ");
      Serial.print("Đã in kênh ");
      Serial.println(i);
    }
    updateLastRelayStatus();
    last_channel = channel;
  }
}

void checkpulse2() // chương trình encoder của CH
{
  static int value = 0;
  // You have to run getRotation() very frequently in loop to prevent missing rotary encoder signals
  // If this is not possible take a look at the pinChangeInterrupt examples
  switch (g_rotaryEncoder.getRotation())
  {
  case KY040::CLOCKWISE:
    encoderPos++;

    if (encoderPos >= DISPLAY_SCREEN_COUNT) //(>4)
      encoderPos = 0;
    flag_encoderPos = encoderPos;
    Serial.print("Direction: ");
    Serial.print(currentDir);
    Serial.print(" | Counter: ");
    Serial.println(encoderPos);
    Serial.print(" | Channel: ");
    Serial.println(flag_encoderPos);
    // ui32_timeout_mqtt=millis()+5000;
    timeout_readpzem = millis() + 2000;
    break;
  case KY040::COUNTERCLOCKWISE:
    encoderPos--;
    if (encoderPos < 0)
      encoderPos = DISPLAY_SCREEN_COUNT - 1; // 4
    flag_encoderPos = encoderPos;

    Serial.print("Direction: ");
    Serial.print(currentDir);
    Serial.print(" | Counter: ");
    Serial.println(encoderPos);
    Serial.print(" | Channel: ");
    Serial.println(flag_encoderPos);
    // ui32_timeout_mqtt=millis()+5000;
    timeout_readpzem = millis() + 2000;
    break;
  }
}

void menu_set()
{
  switch (g_rotaryEncoder.getRotation())
  {
  case KY040::CLOCKWISE:
    if (uiMode == UI_MODE_VIEW)
    {
      // Xoay để đổi màn hình (như code cũ)
      encoderPos++;
      if (encoderPos >= DISPLAY_SCREEN_COUNT)
        encoderPos = 0;
      flag_encoderPos = encoderPos;
      Serial.print("Direction: ");
      Serial.print(currentDir);
      Serial.print(" | Counter: ");
      Serial.println(encoderPos);
      Serial.print(" | Channel: ");
      Serial.println(flag_encoderPos);
      timeout_readpzem = millis() + 2000;
    }
    else if (uiMode == UI_MODE_MENU)
    {
      // Xoay để đổi lựa chọn menu
      menuIndex++;
      if (menuIndex >= MAIN_MENU_ITEMS)
        menuIndex = 0;
    }
    else if (uiMode == UI_MODE_NET_IP_CONFIG)
    {
      netFieldIndex++;
      if (netFieldIndex >= NET_MENU_ITEMS)
      {
        netFieldIndex = 0;
      }
    }
    break;

  case KY040::COUNTERCLOCKWISE:
    if (uiMode == UI_MODE_VIEW)
    {
      encoderPos--;
      if (encoderPos < 0)
        encoderPos = DISPLAY_SCREEN_COUNT - 1;
      flag_encoderPos = encoderPos;
      Serial.print("Direction: ");
      Serial.print(currentDir);
      Serial.print(" | Counter: ");
      Serial.println(encoderPos);
      Serial.print(" | Channel: ");
      Serial.println(flag_encoderPos);
      timeout_readpzem = millis() + 2000;
    }
    else if (uiMode == UI_MODE_MENU)
    {
      menuIndex--;
      if (menuIndex < 0)
        menuIndex = MAIN_MENU_ITEMS - 1;
    }
    else if (uiMode == UI_MODE_NET_IP_CONFIG)
    {
      netFieldIndex--;
      if (netFieldIndex < 0)
      {
        netFieldIndex = NET_MENU_ITEMS - 1;
      }
    }
    break;

  default:
    break;
  }
}

inline void lcdv2_begin()
{
  lcd.init();
  lcd.backlight();
  lcd.clear();

#ifndef LCDV2_EMBEDDED
  UART_PZEM.begin(9600);
  pzem.begin();
#endif

  pinMode(SW_PIN, INPUT_PULLUP);

  pinALast = digitalRead(CLK_PIN);
  lastStateCLK = pinALast;
  last_channel = -1;
  lastStateCLK = pinALast;
  last_channel = -1;
  for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) // 4
  {
    lastDisplayValid[i] = false;
    lastDisplayValues[i].V = 0;
    lastDisplayValues[i].I = 0;
    lastDisplayValues[i].P = 0;
  }
  lastReadMs = millis() + 1000;
}

//-----------------------------------
#define LCD_TICK_INTERVAL_MS 200UL

inline void lcdv2_tick_display() // hiển thị theo kênh do encoder chọn
{
  checkpulse2(); // doc xung
  lcdv2_handle_button();
  // if (flag_encoderPos < CHANNEL_COUNT) // 4
  // {
  //   lcd_show(flag_encoderPos);
  // }
  // else if (flag_encoderPos == CHANNEL_COUNT) // 4
  // {
  //   lcdshowchannelstate(flag_encoderPos);
  // }
  // // lcd_show(flag_encoderPos);

  // Đọc nút + encoder mỗi lần tick

  if (uiMode == UI_MODE_VIEW)
  {
    if (flag_encoderPos < CHANNEL_COUNT)
    {
      lcd_show(flag_encoderPos); // Total + từng CHn
    }
    else if (flag_encoderPos == CHANNEL_COUNT)
    {
      lcdshowchannelstate(flag_encoderPos); // trang trạng thái relay
    }
  }
  else if (uiMode == UI_MODE_MENU)
  {
    lcdv2_show_main_menu();
  }
  else if (uiMode == UI_MODE_NET_MENU)
  {
    lcdv2_show_net_menu();
  }
  else if (uiMode == UI_MODE_NET_IP_CONFIG)
  {
    lcdv2_show_net_ip_config();
  }
}

inline void lcdv2_tick_standalone() // đọc ch[0..4] mỗi 500ms
{
  if (lastReadMs <= millis())
  {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
      read_pzem_channel(i, ch[i]);
    }
    lastReadMs = millis() + 500;
  }
}

// Hàm đọc nút nhấn để vào/ra menu
inline void lcdv2_handle_button()
{
  int btnState = digitalRead(SW_PIN);
  static int lastBtnState = HIGH;

  // phát hiện cạnh xuống: HIGH -> LOW (nhấn nút)
  if (btnState == LOW && lastBtnState == HIGH)
  {
    // Khử nảy bằng millis
    if (millis() - lastButtonPress > BUTTON_DEBOUNCE_MS)
    {
      lastButtonPress = millis();

      // ====== TỪ VIEW → MENU ======
      if (uiMode == UI_MODE_VIEW)
      {
        uiMode = UI_MODE_MENU;
        menuIndex = 0;
        lcd.clear();
        return;
      }

      // ====== ĐANG Ở MENU ======
      else if (uiMode == UI_MODE_MENU)
      {
        switch (menuIndex)
        {
        case 0: // VIEW
          uiMode = UI_MODE_VIEW;
          lcd.clear();
          break;

        case 1: // NET_MENU
          uiMode = UI_MODE_NET_MENU;
          lcd.clear();
          break;

        case 2: // NET CONFIG
          uiMode = UI_MODE_NET_IP_CONFIG;
          netFieldIndex = 0;
          lcd.clear();
          break;

        case 3: // EXIT
          uiMode = UI_MODE_VIEW;
          lcd.clear();
          break;
        }
      }
      // ====== THOÁT MENU NET ======
      else if (uiMode == UI_MODE_NET_IP_CONFIG)
      {
        uiMode = UI_MODE_MENU;
        lcd.clear();
      }
    }
  }
  // cập nhật trạng thái nút **sau khi** xử lý
  lastBtnState = btnState;
}

inline void lcdv2_show_main_menu()
{
  lcdv2_print_line(0, "MENU:");

  switch (menuIndex)
  {
  case 0:
    lcdv2_print_line(1, "> VIEW"); // Xem số liệu (return về UI_MODE_VIEW)
    break;
  case 1:
    lcdv2_print_line(1, "> NET CONFIG");
    break;
  case 2:
    lcdv2_print_line(1, "> EXIT");
    break;
  }
}

inline void lcdv2_show_net_menu()
{
  {
    lcdv2_print_line(0, "NET CONFIG");
    lcdv2_print_line(1, "Rotate to view");
  }
}

inline void lcdv2_show_net_ip_config()
{
  char buffer[17];
  buffer[0] = '\0';

  switch (netFieldIndex)
  {
  case 0:
    lcdv2_print_line(0, "IP ADDRESS");
    lcdv2_format_ip(Ethernet.localIP(), buffer, sizeof(buffer));
    lcdv2_print_line(1, buffer);
    break;
  case 1:
    lcdv2_print_line(0, "GATEWAY");
    lcdv2_format_ip(gateway, buffer, sizeof(buffer));
    lcdv2_print_line(1, buffer);
    break;
  case 2:
    lcdv2_print_line(0, "SUBNET");
    lcdv2_format_ip(subnet, buffer, sizeof(buffer));
    lcdv2_print_line(1, buffer);
    break;
  case 3:
    lcdv2_print_line(0, "DNS SERVER");
    lcdv2_format_ip(dns, buffer, sizeof(buffer));
    lcdv2_print_line(1, buffer);
    break;
  case 4:
  default:
  {
    char hostLine[17];
    if (mqtt_server != nullptr)
    {
      snprintf(hostLine, sizeof(hostLine), "MQTT:%s", mqtt_server);
    }
    else
    {
      snprintf(hostLine, sizeof(hostLine), "MQTT:N/A");
    }
    lcdv2_print_line(0, hostLine);
    snprintf(buffer, sizeof(buffer), "Port:%d", mqtt_port);
    lcdv2_print_line(1, buffer);
    break;
  }
  }
}

inline void lcdv2_print_line(uint8_t row, const char *text)
{
  lcd.setCursor(0, row);
  lcd.print(text);
  size_t len = strlen(text);
  while (len < 16)
  {
    lcd.print(' ');
    ++len;
  }
}

inline void lcdv2_format_ip(const IPAddress &addr, char *buffer, size_t length)
{
  if (length == 0 || buffer == nullptr)
  {
    return;
  }
  snprintf(buffer, length, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
}

//---------------------------------------------------------------------------------
// update sau
void choptat()
{
  /////////////////Chop chu
  long now = millis();
  if (now - timer_chopchu > 100)
  {
    timer_chopchu = now;
    chopchu++;
    if (chopchu == 2)
      chopchu = 0;
    if (chopchu == 0)
    {
      lcd.setCursor(0, dongchop);
      lcd.print("                ");
    }
  }
}

void SetupLCD_3() // SETUP
{
  if (Setup_2 == 1)
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("NAME SETUP      ");
    else
      lcd.print("CAI DAT TEN     ");
    lcd.setCursor(0, 1);
    if (ngonngu == 0)
      lcd.print("ID SETUP        ");
    else
      lcd.print("CAI DAT MA      ");
  }
  if (Setup_2 == 2)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("NAME SETUP      ");
      else
        lcd.print("CAI DAT TEN     ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("ID SETUP        ");
      else
        lcd.print("CAI DAT MA      ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("ID SETUP        ");
      else
        lcd.print("CAI DAT MA      ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("IP SETUP        ");
      else
        lcd.print("CD DIA CHI      ");
    }
  }
  if (Setup_2 == 3)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("ID SETUP        ");
      else
        lcd.print("CAI DAT MA      ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("IP SETUP        ");
      else
        lcd.print("CD DIA CHI      ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("IP SETUP        ");
      else
        lcd.print("CAI DAT IP      ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("SERVER SETUP    ");
      else
        lcd.print("CD MAY CHU      ");
    }
  }
  if (Setup_2 == 4)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("IP SETUP        ");
      else
        lcd.print("CAI DAT IP      ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("SERVER SETUP    ");
      else
        lcd.print("CD MAY CHU      ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("SERVER SETUP    ");
      else
        lcd.print("CD MAY CHU      ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("EXIT            ");
      else
        lcd.print("THOAT           ");
    }
  }
  if (Setup_2 == 5)
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("SERVER SETUP    ");
    else
      lcd.print("CD MAY CHU      ");
    lcd.setCursor(0, 1);
    if (ngonngu == 0)
      lcd.print("EXIT            ");
    else
      lcd.print("THOAT           ");
  }
}

void SetupLCD_3_1() // NAME SETUP
{
  lcd.setCursor(0, 0);
  if (ngonngu == 0)
    lcd.print("NAME:      ");
  else
    lcd.print("TEN:       ");
  for (int i = 0; i < 5; i++)
  {
    if (Setup_3 < 6)
    {
      lcd.setCursor(11 + i, 0);
      if ((11 + i) != (10 + Setup_3))
      {
        lcd.print(chuChar[Name[i]]);
      }
      else
      {
        long now = millis();
        if (now - timer_chopchu > 100)
        {
          timer_chopchu = now;
          chopchu++;
          if (chopchu == 2)
            chopchu = 0;
          if (chopchu == 0)
          {
            if (setchu == 1)
              lcd.write(255);
            else
              lcd.print("_");
          }
          else
            lcd.print(chuChar[Name[i]]);
        }
      }
    }
    else
    {
      lcd.setCursor(11 + i, 0);
      lcd.print(chuChar[Name[i]]);
    }
  }
  lcd.setCursor(0, 1);
  if (ngonngu == 0)
    lcd.print("EXIT            ");
  else
    lcd.print("THOAT           ");
  ////lcd.noBlink();lcd.blink();
}

/////////////////////////////////////////////////////////////////////////
void SetupLCD_3_3() // IP SETUP
{
  if (Setup_3 == 1) // ip dong
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("IP AUTOMATICALLY");
    lcd.print("IP TU DONG      ");
    lcd.setCursor(0, 1);
    lcd.print("              ");
  }
  else if (Setup_3 == 2) // ip tinh
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("IP MANUALLY     ");
    lcd.print("IP THU CONG     ");
    lcd.setCursor(0, 1);
    lcd.print("              ");
  }
}

/////////////////////////////////////////////////////////////////////////
void SetupLCD_3_3_2()
{
  // Serial.print("setIPtinh: ");Serial.print(setIPtinh);Serial.print("   setgiatriIP: ");Serial.println(setgiatriIP);
  if (Setup_4 == 1) // ipaddress[]
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("IP ADDRESS      ");
    else
      lcd.print("DIA CHI IP      ");
    if (setIPtinh == 0)
    {
      sprintf(temp, "%3d.%3d.%3d.%3d ", ip[0], ip[1], ip[2], ip[3]);
      lcd.setCursor(0, 1);
      lcd.print(temp);
    }
    else // vao set IP address
    {
      if (setIPtinh == 5)
      {
        dongchop = 1;
        choptat();
        if (chopchu > 0)
        {
          lcd.setCursor(0, 1);
          if (ngonngu == 0)
            lcd.print("EXIT            ");
          else
            lcd.print("THOAT           ");
        }
      }
      else
      {
        for (int i = 1; i < 5; i++)
        {
          lcd.setCursor((i * 4) - 4, 1);
          if (setIPtinh == i)
          {
            long now = millis();
            if (now - timer_chopchu > 1000)
            {
              timer_chopchu = now;
              chopchu++;
              if (chopchu == 2)
                chopchu = 0;
            }
            if (chopchu == 0)
            {
              if (setgiatriIP == 256)
              {
                lcd.print("___");
              }
              else
              {
                lcd.write(255);
                lcd.write(255);
                lcd.write(255);
              }
            }
            else
            {
              if (setgiatriIP == 256)
              {
                sprintf(temp, "%3d", ip[i - 1]);
              }
              else
              {
                sprintf(temp, "%3d", setgiatriIP);
              }
              lcd.print(temp);
            }
          }
          else
          {
            sprintf(temp, "%3d", ip[i - 1]);
            lcd.print(temp);
          }
          if (i != 4)
          {
            lcd.print(".");
          }
        }
      }
    }
  }
  else if (Setup_4 == 2) // gateway[]
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("DEFAULT GATEWAY ");
    else
      lcd.print("CONG MAC DINH   ");
    if (setIPtinh == 0)
    {
      sprintf(temp, "%3d.%3d.%3d.%3d ", gateway[0], gateway[1], gateway[2], gateway[3]);
      lcd.setCursor(0, 1);
      lcd.print(temp);
    }
    else // vao set GATEWAY
    {
      if (setIPtinh == 5)
      {
        dongchop = 1;
        choptat();
        if (chopchu > 0)
        {
          lcd.setCursor(0, 1);
          if (ngonngu == 0)
            lcd.print("EXIT            ");
          else
            lcd.print("THOAT           ");
        }
      }
      else
      {
        for (int i = 1; i < 5; i++)
        {
          lcd.setCursor((i * 4) - 4, 1);
          if (setIPtinh == i)
          {
            long now = millis();
            if (now - timer_chopchu > 1000)
            {
              timer_chopchu = now;
              chopchu++;
              if (chopchu == 2)
                chopchu = 0;
            }
            if (chopchu == 0)
            {
              if (setgiatriIP == 256)
              {
                lcd.print("___");
              }
              else
              {
                lcd.write(255);
                lcd.write(255);
                lcd.write(255);
              }
            }
            else
            {
              if (setgiatriIP == 256)
              {
                sprintf(temp, "%3d", gateway[i - 1]);
              }
              else
              {
                sprintf(temp, "%3d", setgiatriIP);
              }
              lcd.print(temp);
            }
          }
          else
          {
            sprintf(temp, "%3d", gateway[i - 1]);
            lcd.print(temp);
          }
          if (i != 4)
          {
            lcd.print(".");
          }
        }
      }
    }
  }
  else if (Setup_4 == 3) // subnet[]
  {

    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("SUBNET MASK     ");
    else
      lcd.print("MAT NA MANG CON ");
    if (setIPtinh == 0)
    {
      sprintf(temp, "%3d.%3d.%3d.%3d ", subnet[0], subnet[1], subnet[2], subnet[3]);
      lcd.setCursor(0, 1);
      lcd.print(temp);
    }
    else // vao set subnet
    {
      if (setIPtinh == 5)
      {
        dongchop = 1;
        choptat();
        if (chopchu > 0)
        {
          lcd.setCursor(0, 1);
          if (ngonngu == 0)
            lcd.print("EXIT            ");
          else
            lcd.print("THOAT           ");
        }
      }
      else
      {
        for (int i = 1; i < 5; i++)
        {
          lcd.setCursor((i * 4) - 4, 1);
          if (setIPtinh == i)
          {
            long now = millis();
            if (now - timer_chopchu > 1000)
            {
              timer_chopchu = now;
              chopchu++;
              if (chopchu == 2)
                chopchu = 0;
            }
            if (chopchu == 0)
            {
              if (setgiatriIP == 256)
              {
                lcd.print("___");
              }
              else
              {
                lcd.write(255);
                lcd.write(255);
                lcd.write(255);
              }
            }
            else
            {
              if (setgiatriIP == 256)
              {
                sprintf(temp, "%3d", subnet[i - 1]);
              }
              else
              {
                sprintf(temp, "%3d", setgiatriIP);
              }
              lcd.print(temp);
            }
          }
          else
          {
            sprintf(temp, "%3d", subnet[i - 1]);
            lcd.print(temp);
          }
          if (i != 4)
          {
            lcd.print(".");
          }
        }
      }
    }
  }
  else if (Setup_4 == 4)
  {
    dongchop = 0;
    choptat();
    if (chopchu > 0)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("EXIT            ");
      else
        lcd.print("THOAT           ");
      lcd.setCursor(0, 1);
      lcd.print("                ");
    }
  }
}
