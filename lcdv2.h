#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Ethernet.h>
#include "HC4052.h"
#include <string.h>
#include "Hshopvn_Pzem004t_V2.h"
#include <KY040.h>

extern IPAddress ip;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress dns;
extern int mqtt_port;
extern const char *mqtt_server;

void apply_network_settings();

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
// Lưu vết mục NET CONFIG đã vẽ lần cuối để tránh refresh không cần thiết
int lastNetCfgIndex = -1;

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

bool netEditing = false;
uint8_t netEditDigitIndex = 0;
uint8_t netEditBuffer[4] = {0};
char netEditString[16] = {0};
IPAddress *netEditTarget = nullptr;
bool netConfirming = false;      // đang ở màn hình OK / EXIT
uint8_t netConfirmSelection = 0; // 0 = OK, 1 = EXIT

void lcdv2_handle_button();
void lcdv2_show_main_menu();
void lcdv2_show_net_menu();
void lcdv2_show_net_ip_config();
void lcdv2_print_line(uint8_t row, const char *text);
void lcdv2_format_ip(const IPAddress &addr, char *buffer, size_t length);
IPAddress *lcdv2_get_selected_address();
void lcdv2_start_edit_net_field();
void lcdv2_commit_net_octet();
void lcdv2_finish_net_edit();
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

// void checkpulse2() // chương trình encoder của CH
// {
//   static int value = 0;
//   // You have to run getRotation() very frequently in loop to prevent missing rotary encoder signals
//   // If this is not possible take a look at the pinChangeInterrupt examples
//   switch (g_rotaryEncoder.getRotation())
//   {
//   case KY040::CLOCKWISE:
//     encoderPos++;

//     if (encoderPos >= DISPLAY_SCREEN_COUNT) //(>4)
//       encoderPos = 0;
//     flag_encoderPos = encoderPos;
//     Serial.print("Direction: ");
//     Serial.print(currentDir);
//     Serial.print(" | Counter: ");
//     Serial.println(encoderPos);
//     Serial.print(" | Channel: ");
//     Serial.println(flag_encoderPos);
//     // ui32_timeout_mqtt=millis()+5000;
//     timeout_readpzem = millis() + 2000;
//     break;
//   case KY040::COUNTERCLOCKWISE:
//     encoderPos--;
//     if (encoderPos < 0)
//       encoderPos = DISPLAY_SCREEN_COUNT - 1; // 4
//     flag_encoderPos = encoderPos;

//     Serial.print("Direction: ");
//     Serial.print(currentDir);
//     Serial.print(" | Counter: ");
//     Serial.println(encoderPos);
//     Serial.print(" | Channel: ");
//     Serial.println(flag_encoderPos);
//     // ui32_timeout_mqtt=millis()+5000;
//     timeout_readpzem = millis() + 2000;
//     break;
//   }
// }

void checkpulse2() // chương trình encoder: VIEW + MENU + NET
{
  static int value = 0; // bạn đang để, cứ giữ cũng không sao

  int step = 0;

  // Đọc trạng thái xoay từ thư viện KY040 (giống code cũ)
  switch (g_rotaryEncoder.getRotation())
  {
  case KY040::CLOCKWISE:
    step = +1;
    break;
  case KY040::COUNTERCLOCKWISE:
    step = -1;
    break;
  default:
    return; // không xoay thì thôi
  }

  // ===== ĐANG CHỈNH TỪNG CHỮ SỐ ĐỊA CHỈ MẠNG =====
  if (netEditing)
  {
    if (netEditString[netEditDigitIndex] == '.')
    {
      return;
    }

    int digit = netEditString[netEditDigitIndex] - '0';
    // xoay encoder để tăng/giảm từng chữ số 0..9 (quay vòng)
    digit = (digit + step + 10) % 10;
    netEditString[netEditDigitIndex] = static_cast<char>('0' + digit);

    // 👉 VẼ TRỰC TIẾP KÝ TỰ VỪA ĐỔI LÊN LCD CHO NHẠY
    lcd.setCursor(netEditDigitIndex, 1);
    lcd.print(netEditString[netEditDigitIndex]);
    // đặt lại con trỏ tại vị trí đang edit
    lcd.setCursor(netEditDigitIndex, 1);
    lcd.cursor();

    return;
  }

  // ===== ĐANG Ở MÀN HÌNH CONFIRM (OK / EXIT) =====
  if (uiMode == UI_MODE_NET_IP_CONFIG && netConfirming)
  {
    // chỉ xử lý khi thực sự có step
    if (step != 0)
    {
      // 0 <-> 1 (OK <-> EXIT)
      netConfirmSelection ^= 1;

      // vẽ lại ngay màn hình OK / EXIT
      lcdv2_show_net_ip_config();
    }
    return; // không cho rơi xuống các mode khác
  }

  // ===== VIEW MODE: giữ behavior cũ để đổi màn hình CH =====
  if (uiMode == UI_MODE_VIEW)
  {
    if (step > 0)
    {
      encoderPos++;

      if (encoderPos >= DISPLAY_SCREEN_COUNT) //(>4)
        encoderPos = 0;
    }
    else // step < 0
    {
      encoderPos--;
      if (encoderPos < 0)
        encoderPos = DISPLAY_SCREEN_COUNT - 1; // 4
    }

    flag_encoderPos = encoderPos;
    Serial.print("Direction: ");
    Serial.print(currentDir);
    Serial.print(" | Counter: ");
    Serial.println(encoderPos);
    Serial.print(" | Channel: ");
    Serial.println(flag_encoderPos);
    // ui32_timeout_mqtt=millis()+5000;
    timeout_readpzem = millis() + 2000;
    return;
  }

  // ===== MENU MODE: xoay để đổi lựa chọn menu chính =====
  if (uiMode == UI_MODE_MENU)
  {
    menuIndex += step;

    if (menuIndex >= MAIN_MENU_ITEMS)
      menuIndex = 0;
    else if (menuIndex < 0)
      menuIndex = MAIN_MENU_ITEMS - 1;

    // Serial.print("MENU step="); Serial.print(step);
    // Serial.print("  menuIndex="); Serial.println(menuIndex);
    return;
  }

  // ===== NET MENU & NET IP CONFIG: xoay để đổi IP/GW/Subnet/DNS/MQTT =====
  if (uiMode == UI_MODE_NET_MENU || uiMode == UI_MODE_NET_IP_CONFIG)
  {
    netFieldIndex += step;

    if (netFieldIndex >= NET_MENU_ITEMS)
      netFieldIndex = 0;
    else if (netFieldIndex < 0)
      netFieldIndex = NET_MENU_ITEMS - 1;

    // Serial.print("NET step="); Serial.print(step);
    // Serial.print("  netFieldIndex="); Serial.println(netFieldIndex);
    return;
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

  // Ở VIEW: dùng logic cũ, mượt, CH
  checkpulse2();

  // ---- XỬ LÝ NÚT NHẤN ----
  lcdv2_handle_button();

  // ---- VẼ LCD THEO uiMode ----
  if (uiMode == UI_MODE_VIEW)
  {
    if (flag_encoderPos < CHANNEL_COUNT)
    {
      lcd_show(flag_encoderPos); // Total + CH1..CH4
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
          netFieldIndex = 0;
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
      else if (uiMode == UI_MODE_NET_MENU)
      {
        uiMode = UI_MODE_MENU;
        lcd.clear();
      }
      // ====== THOÁT MENU NET ======
      else if (uiMode == UI_MODE_NET_IP_CONFIG) // <== CHỈ NET CONFIG
      {
        if (netEditing)
        {
          // đang nhập từng digit → next digit / sang confirm
          lcdv2_commit_net_octet();
        }
        else if (netConfirming)
        {
          // đang ở màn OK / EXIT
          if (netConfirmSelection == 0)
          {
            // OK
            lcdv2_finish_net_edit();
          }
          else
          {
            // EXIT: bỏ thay đổi
            netConfirming = false;
            netEditTarget = nullptr;
            lastNetCfgIndex = -1;
          }
        }
        else if (netFieldIndex < 4)
        {
          // bắt đầu chỉnh IP/GW/Subnet/DNS
          lcdv2_start_edit_net_field();
        }
        else
        {
          // mục MQTT hoặc mục cuối → thoát về MENU
          uiMode = UI_MODE_MENU;
          lcd.clear();
        }
      }
    }
  }
  // cập nhật trạng thái nút **sau khi** xử lý
  lastBtnState = btnState;
}

inline void lcdv2_show_main_menu()
{
  static int lastMenuIndex = -1;

  // Nếu không đổi menuIndex thì khỏi vẽ lại → tiết kiệm thời gian
  if (menuIndex == lastMenuIndex)
  {
    return;
  }
  lastMenuIndex = menuIndex;

  lcdv2_print_line(0, "MENU:");

  switch (menuIndex)
  {
  case 0:
    lcdv2_print_line(1, "> VIEW"); // Xem số liệu (return về UI_MODE_VIEW)
    break;
  case 1:
    lcdv2_print_line(1, "> NET VIEW");
    break;
  case 2:
    lcdv2_print_line(1, "> NET CONFIG");
    break;
  case 3:
    lcdv2_print_line(1, "> EXIT");
    break;
  }
}

inline void lcdv2_show_net_menu()
{
  static int lastNetIndex = -1;
  if (netFieldIndex == lastNetIndex)
  {
    return; // không đổi mục → khỏi vẽ lại
  }
  lastNetIndex = netFieldIndex;

  char buffer[17];
  buffer[0] = '\0';

  switch (netFieldIndex)
  {
  case 0: // IP đang chạy
    lcdv2_print_line(0, "IP (VIEW)");
    lcdv2_format_ip(Ethernet.localIP(), buffer, sizeof(buffer));
    lcdv2_print_line(1, buffer);
    break;

  case 1: // Gateway cấu hình
    lcdv2_print_line(0, "GATEWAY (VIEW)");
    lcdv2_format_ip(gateway, buffer, sizeof(buffer));
    lcdv2_print_line(1, buffer);
    break;

  case 2: // Subnet cấu hình
    lcdv2_print_line(0, "SUBNET (VIEW)");
    lcdv2_format_ip(subnet, buffer, sizeof(buffer));
    lcdv2_print_line(1, buffer);
    break;

  case 3: // DNS cấu hình
    lcdv2_print_line(0, "DNS (VIEW)");
    lcdv2_format_ip(dns, buffer, sizeof(buffer));
    lcdv2_print_line(1, buffer);
    break;

  case 4: // MQTT port
  default:
    lcdv2_print_line(0, "MQTT VIEW");
    snprintf(buffer, sizeof(buffer), "Port:%d", mqtt_port);
    lcdv2_print_line(1, buffer);
    break;
  }
}

inline void lcdv2_show_net_ip_config()
{
  static bool lastEditing = false;
  static bool lastConfirming = false;
  static uint8_t lastConfirmSelection = 0;
  static uint8_t lastEditDigitIndex = 0;
  static char lastEditString[16] = "";

  const bool editStringChanged = netEditing && (strcmp(lastEditString, netEditString) != 0);
  const bool digitMoved = netEditing && (netEditDigitIndex != lastEditDigitIndex);
  const bool confirmingChanged = netConfirming && (netConfirmSelection != lastConfirmSelection);
  const bool needRedraw = (netFieldIndex != lastNetCfgIndex) ||
                          (netEditing != lastEditing) ||
                          (netConfirming != lastConfirming) ||
                          editStringChanged ||
                          digitMoved ||
                          confirmingChanged;

  if (!needRedraw)
  {
    // giữ nguyên con trỏ khi không cần vẽ lại toàn màn hình
    if (netEditing)
    {
      lcd.setCursor(netEditDigitIndex, 1);
      lcd.cursor();
    }
    else if (netConfirming)
    {
      lcd.setCursor(netConfirmSelection == 0 ? 0 : 6, 1);
      lcd.cursor();
    }
    return;
  }

  lastNetCfgIndex = netFieldIndex;
  lastEditing = netEditing;
  lastConfirming = netConfirming;
  lastConfirmSelection = netConfirmSelection;
  lastEditDigitIndex = netEditDigitIndex;
  if (netEditing)
  {
    strncpy(lastEditString, netEditString, sizeof(lastEditString));
    lastEditString[sizeof(lastEditString) - 1] = '\0';
  }
  else
  {
    lastEditString[0] = '\0';
  }

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
    lcdv2_print_line(0, hostLine);
    snprintf(buffer, sizeof(buffer), "Port:%d", mqtt_port);
    lcdv2_print_line(1, buffer);
    break;
  }
  }

  // --- ĐANG EDIT TỪNG CHỮ SỐ ---
  if (netEditing)
  {
    const char *titles[] = {"SUA IP", "SUA GW", "SUA SUB", "SUA DNS", "MQTT"};
    lcdv2_print_line(0, titles[netFieldIndex]);
    lcdv2_print_line(1, netEditString);

    lcd.setCursor(netEditDigitIndex, 1);
    lcd.cursor();
    return;
  }

  lcd.noCursor();

  // --- ĐANG Ở MÀN HÌNH OK / EXIT ---
  if (netConfirming)
  {
    lcdv2_print_line(0, "OK?");
    lcdv2_print_line(1, "OK   EXIT");

    if (netConfirmSelection == 0)
    {
      lcd.setCursor(0, 1); // trỏ vào "OK"
    }
    else
    {
      lcd.setCursor(6, 1); // trỏ vào "EXIT"
    }
    lcd.cursor();
    return;
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
  if (!buffer || length == 0)
    return;

  snprintf(buffer, length, "%u.%u.%u.%u",
           addr[0], addr[1], addr[2], addr[3]);
}

inline IPAddress *lcdv2_get_selected_address()
{
  switch (netFieldIndex)
  {
  case 0:
    return &ip;
  case 1:
    return &gateway;
  case 2:
    return &subnet;
  case 3:
    return &dns;
  default:
    return nullptr;
  }
}

inline void lcdv2_start_edit_net_field()
{
  IPAddress *target = lcdv2_get_selected_address();
  if (target == nullptr)
  {
    return;
  }

  netEditTarget = target;

  for (uint8_t i = 0; i < 4; ++i)
  {
    netEditBuffer[i] = (*netEditTarget)[i];
  }

  snprintf(netEditString, sizeof(netEditString), "%03u.%03u.%03u.%03u", netEditBuffer[0], netEditBuffer[1], netEditBuffer[2], netEditBuffer[3]);

  netEditDigitIndex = 0;
  netEditing = true;
  lastNetCfgIndex = -1; // buộc vẽ lại
}

inline void lcdv2_finish_net_edit()
{
  if (netEditTarget == nullptr)
  {
    netEditing = false;
    netConfirming = false;
    return;
  }

  // parse chuỗi "NNN.NNN.NNN.NNN" thành 4 octet
  netEditBuffer[0] = static_cast<uint8_t>(
      (netEditString[0] - '0') * 100 +
      (netEditString[1] - '0') * 10 +
      (netEditString[2] - '0'));

  netEditBuffer[1] = static_cast<uint8_t>(
      (netEditString[4] - '0') * 100 +
      (netEditString[5] - '0') * 10 +
      (netEditString[6] - '0'));

  netEditBuffer[2] = static_cast<uint8_t>(
      (netEditString[8] - '0') * 100 +
      (netEditString[9] - '0') * 10 +
      (netEditString[10] - '0'));

  netEditBuffer[3] = static_cast<uint8_t>(
      (netEditString[12] - '0') * 100 +
      (netEditString[13] - '0') * 10 +
      (netEditString[14] - '0'));

  for (uint8_t i = 0; i < 4; ++i)
  {
    if (netEditBuffer[i] > 255)
      netEditBuffer[i] = 255;
  }

  *netEditTarget = IPAddress(netEditBuffer[0], netEditBuffer[1],
                             netEditBuffer[2], netEditBuffer[3]);

  // áp dụng lại cấu hình Ethernet thực tế
  apply_network_settings();

  netEditing = false;
  netConfirming = false;
  netConfirmSelection = 0;
  netEditTarget = nullptr;
  netEditDigitIndex = 0;
  lastNetCfgIndex = -1;
  lcd.noCursor();
}

inline void lcdv2_commit_net_octet()
{
  if (!netEditing)
    return;

  uint8_t len = strlen(netEditString);

  // nhảy qua các vị trí '.' tới digit tiếp theo
  do
  {
    ++netEditDigitIndex;
  } while (netEditDigitIndex < len && netEditString[netEditDigitIndex] == '.');

  // nếu đã hết tất cả digit → chuyển sang màn hình confirm OK / EXIT
  if (netEditDigitIndex >= len)
  {
    netEditing = false;
    netConfirming = true;
    netConfirmSelection = 0; // mặc định chọn OK
    lcd.noCursor();
    lastNetCfgIndex = -1; // buộc vẽ lại màn hình OK/EXIT
    return;
  }

  // còn digit → đưa con trỏ tới đó
  lcd.setCursor(netEditDigitIndex, 1);
}
