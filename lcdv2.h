#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Ethernet.h>
#include "HC4052.h"
#include <string.h>
#include "Hshopvn_Pzem004t_V2.h"
#include <ClickEncoder.h>
#include <TimerOne.h>

extern IPAddress ip;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress dns;
// extern int mqtt_port;
extern int mqtt_port;
extern char mqtt_server[];
extern bool telemetryDirty;
extern uint8_t nextPublishChannel;

void apply_network_settings();
void persistNetworkConfig();

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define CLK_PIN 47 // aka. A
#define DT_PIN 48  // aka. BF

// Con trỏ encoder dùng chung cho lcdv2
ClickEncoder *g_encoder = nullptr;

// ISR gọi mỗi 1ms để phục vụ ClickEncoder
void lcdv2_encoder_isr()
{
  if (g_encoder)
  {
    g_encoder->service();
  }
}

unsigned long lastTelemetryMs = 0;
int encoderPos = 0;
int flag_encoderPos = 0;
unsigned long lastReadMs = 0;
#define MUX_A 28
#define MUX_B 27
HC4052 mux(MUX_A, MUX_B);
const int Auto = 30; // Doc trang thai Auto Man
const int Man = 31;
// Pin nút nhấn encoder
#define SW_PIN 49
unsigned long ui32_timeout_mqtt;
unsigned long timeout_readpzem;
const uint8_t RELAY_COUNT = 4;
const uint8_t CHANNEL_COUNT = 5;        // tổng cộng 5 kênh tính cả kênh 0 (Tatal CH)
const uint8_t DISPLAY_SCREEN_COUNT = 6; // 5 kênh đo + 1 trang trạng thái relay
bool autoModeEnabled;
int lastRelayStates[RELAY_COUNT] = {-1};
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

const unsigned long BUTTON_DEBOUNCE_MS = 20;
const unsigned long BUTTON_LONG_PRESS_MS = 2000;  // 2 giây nhấn giữ để thoát menu
const unsigned long NET_CONFIG_TIMEOUT_MS = 60000; // 1 phút không thao tác sẽ thoát 60000UL

bool netEditing = false;
uint8_t netEditDigitIndex = 0;
uint8_t netEditBuffer[4] = {0};
char netEditString[16] = {0};
IPAddress *netEditTarget = nullptr;
bool netConfirming = false;      // đang ở màn hình OK / EXIT
uint8_t netConfirmSelection = 0; // 0 = OK, 1 = EXIT
unsigned long netConfigLastInteractionMs = 0;

void lcdv2_handle_button();
void lcdv2_show_main_menu();
void lcdv2_show_net_menu();
void lcdv2_show_net_ip_config();
void lcdv2_print_line(uint8_t row, const char *text);
void lcdv2_format_ip(const IPAddress &addr, char *buffer, size_t length);
void lcdv2_show_hint(const char *line1, const char *line2);
IPAddress *lcdv2_get_selected_address();
void lcdv2_start_edit_net_field();
void lcdv2_commit_net_octet();
void lcdv2_finish_net_edit();
void lcdv2_reset_net_state();
void lcdv2_timeout();
void lcdv2_handle_net_timeout();
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
      // Serial.print("Đã in kênh ");
      // Serial.println(i);
    }
    updateLastRelayStatus();
    last_channel = channel;
  }
}

void checkpulse2() // chương trình encoder: VIEW + MENU + NET (dùng ClickEncoder)
{
  // rawAccum: cộng dồn step thô của ClickEncoder
  static int16_t rawAccum = 0;

  if (!g_encoder)
  {
    return;
  }

  // Δstep đọc được từ encoder (mỗi lần Timer1 gọi service())
  int16_t delta = g_encoder->getValue();
  if (delta == 0)
  {
    return; // không xoay thì thôi
  }

  rawAccum += delta;

  // MỖI 4 STEP THÔ MỚI TÍNH LÀ 1 BƯỚC LOGIC → GIẢM ĐỘ NHẠY
  const int STEPS_PER_NOTCH = 3; // có thể đổi 2 nếu muốn nhạy hơn

  int step = 0;

  while (rawAccum >= STEPS_PER_NOTCH)
  {
    step++;
    rawAccum -= STEPS_PER_NOTCH;
  }
  while (rawAccum <= -STEPS_PER_NOTCH)
  {
    step--;
    rawAccum += STEPS_PER_NOTCH;
  }

  if (step == 0)
  {
    return; // xoay ít quá, chưa đủ 4 step → bỏ qua
  }

  lcdv2_timeout();

  // ===== ĐANG CHỈNH TỪNG CHỮ SỐ ĐỊA CHỈ MẠNG =====
  if (netEditing)
  {
    if (netEditString[netEditDigitIndex] == '.')
    {
      return;
    }

    int digit = netEditString[netEditDigitIndex] - '0';
    digit = (digit + step + 10) % 10; // quay vòng 0..9
    netEditString[netEditDigitIndex] = static_cast<char>('0' + digit);

    // Vẽ trực tiếp ký tự vừa đổi cho nhạy
    lcd.setCursor(netEditDigitIndex, 1);
    lcd.print(netEditString[netEditDigitIndex]);
    lcd.setCursor(netEditDigitIndex, 1);
    lcd.cursor();
    return;
  }

  // ===== ĐANG Ở MÀN HÌNH CONFIRM (OK / EXIT) =====
  if (uiMode == UI_MODE_NET_IP_CONFIG && netConfirming)
  {
    if (step != 0)
    {
      // 0 <-> 1 (OK <-> EXIT)
      netConfirmSelection ^= 1;
      lcdv2_show_net_ip_config();
    }
    return;
  }

  // ===== VIEW MODE: đổi kênh CH =====
  if (uiMode == UI_MODE_VIEW)
  {
    if (step > 0)
    {
      encoderPos++;
      if (encoderPos >= DISPLAY_SCREEN_COUNT)
        encoderPos = 0;
    }
    else
    {
      encoderPos--;
      if (encoderPos < 0)
        encoderPos = DISPLAY_SCREEN_COUNT - 1;
    }

    flag_encoderPos = encoderPos; // Đồng bộ biến dùng để hiển thị
    // Serial.print("Direction: ");
    // Serial.print(currentDir);
    // Serial.print(" | Counter: ");
    // Serial.println(encoderPos);
    // Serial.print(" | Channel: ");
    // Serial.println(flag_encoderPos);

    timeout_readpzem = millis() + 2000;
    return;
  }

  // ===== MENU MODE: xoay để đổi lựa chọn menu chính =====
  if (uiMode == UI_MODE_MENU)
  {
    lcdv2_timeout();
    menuIndex += step;

    if (menuIndex >= MAIN_MENU_ITEMS)
      menuIndex = 0;
    else if (menuIndex < 0)
      menuIndex = MAIN_MENU_ITEMS - 1;

    last_channel = -1; // buộc vẽ lại menu khi quay từ VIEW sang MENU
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

  // Nút nhấn encoder vẫn dùng INPUT_PULLUP như cũ
  pinMode(SW_PIN, INPUT_PULLUP);

  // --- KHỞI TẠO CLICKENCODER ---
  // Thứ tự: (DT, CLK, SW)
  g_encoder = new ClickEncoder(DT_PIN, CLK_PIN, SW_PIN);

  // Giảm độ nhạy: tắt acceleration để mỗi nấc nhảy đều
  g_encoder->setAccelerationEnabled(false);

  // --- TIMER1 GỌI service() MỖI 1ms ---
  Timer1.initialize(1000); // 1000us = 1ms
  Timer1.attachInterrupt(lcdv2_encoder_isr);

  last_channel = -1;

  for (uint8_t i = 0; i < CHANNEL_COUNT; ++i)
  {
    lastDisplayValid[i] = false;
    lastDisplayValues[i].V = 0;
    lastDisplayValues[i].I = 0;
    lastDisplayValues[i].P = 0;
  }
  // lastReadMs = millis() + 1000;
}

//-----------------------------------
#define LCD_TICK_INTERVAL_MS 200UL

inline void lcdv2_tick_display() // hiển thị theo kênh do encoder chọn
{

  // // Ở VIEW: dùng logic cũ, mượt, CH
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
  lcdv2_handle_net_timeout();
}

// inline void lcdv2_tick_standalone() // đọc ch[0..4] mỗi 500ms
// {
//   if (lastReadMs <= millis())
//   {
//     for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
//     {
//       read_pzem_channel(i, ch[i]);
//     }
//     lastReadMs = millis() + 500;
//   }
// }

extern unsigned long lastSensorPollMs;

// Hàm đọc nút nhấn để vào/ra menu
inline void lcdv2_handle_button()
{
  int btnState = digitalRead(SW_PIN);
  static int lastBtnState = HIGH;
  static unsigned long pressStartMs = 0;
  static bool longPressHandled = false;

  unsigned long now = millis();

  // --- PHÁT HIỆN BẮT ĐẦU NHẤN (cạnh xuống) ---
  if (btnState == LOW && lastBtnState == HIGH)
  {
    lastTelemetryMs = millis();
    lastSensorPollMs = millis();
    pressStartMs = now;
    longPressHandled = false; // chuẩn bị cho 1 lần bấm mới

    lcdv2_timeout();
  }

  // --- XỬ LÝ NHẤN GIỮ LÂU (>= 2s) ---
  if (btnState == LOW && !longPressHandled &&
      (now - pressStartMs >= BUTTON_LONG_PRESS_MS))
  {
    lastSensorPollMs = millis();
    // Chỉ thoát menu nếu đang KHÔNG ở VIEW
    if (uiMode != UI_MODE_VIEW)
    {
      // Thoát hết về VIEW
      uiMode = UI_MODE_VIEW;

      // Reset các trạng thái edit/confirm của NET CONFIG
      // netEditing = false;
      // netConfirming = false;
      // netConfirmSelection = 0;
      // netEditTarget = nullptr;
      // netEditDigitIndex = 0;
      // lastNetCfgIndex = -1;

      lcdv2_reset_net_state();

      // lcd.clear();
      lcdv2_show_hint("BACK TO VIEW", "Turn to view CH");
    }

    longPressHandled = true; // Đánh dấu đã xử lý long press
  }

  // --- PHÁT HIỆN NHẢ NÚT (cạnh lên) ---
  if (btnState == HIGH && lastBtnState == LOW)
  {
    lastSensorPollMs = millis();
    // Nếu long-press đã xử lý rồi → KHÔNG làm short-press nữa
    if (!longPressHandled && (now - pressStartMs > BUTTON_DEBOUNCE_MS))
    {
      // ====== XỬ LÝ NHẤN NGẮN ======
      // TỪ VIEW → MENU
      if (uiMode == UI_MODE_VIEW)
      {
        uiMode = UI_MODE_MENU;
        menuIndex = 0;
        // lcd.clear();
        netConfigLastInteractionMs = millis();
        lcdv2_show_hint("MENU", "Turn to select");
      }

      // ĐANG Ở MENU CHÍNH
      else if (uiMode == UI_MODE_MENU)
      {
        switch (menuIndex)
        {
        case 0: // VIEW
          uiMode = UI_MODE_VIEW;
          // lcd.clear();
          netConfigLastInteractionMs = 0;
          lcdv2_show_hint("VIEW MODE", "Turn to change");
          break;

        case 1: // NET_MENU (VIEW thông số mạng)
          uiMode = UI_MODE_NET_MENU;
          netFieldIndex = 0;
          netConfigLastInteractionMs = millis();
          // lcd.clear();
          lcdv2_show_hint("NET VIEW", "Turn to change");
          break;

        case 2: // NET CONFIG (chỉnh IP tĩnh)
          uiMode = UI_MODE_NET_IP_CONFIG;
          netFieldIndex = 0;
          netConfigLastInteractionMs = millis();
          // lcd.clear();
          lcdv2_show_hint("NET CONFIG", "Press to edit IP");
          break;

        case 3: // EXIT về VIEW
          uiMode = UI_MODE_VIEW;
          // lcd.clear();
          netConfigLastInteractionMs = 0;
          lcdv2_show_hint("EXIT MENU", "Turn to view CH");
          break;
        }
      }

      // ĐANG Ở NET MENU (chỉ view thông số mạng)
      else if (uiMode == UI_MODE_NET_MENU)
      {
        // Nhấn ngắn: quay lại MENU
        uiMode = UI_MODE_MENU;
        netConfigLastInteractionMs = millis();
        // lcd.clear();
        lcdv2_show_hint("MENU", "Turn to select");
      }

      // ĐANG Ở NET CONFIG
      else if (uiMode == UI_MODE_NET_IP_CONFIG)
      {
        if (netEditing)
        {
          // đang nhập từng digit → next digit / xong octet thì sang confirm
          lcdv2_commit_net_octet();
        }
        else if (netConfirming)
        {
          // đang ở màn OK / EXIT
          if (netConfirmSelection == 0)
          {
            // OK → lưu IP, ở lại NET CONFIG
            lcdv2_finish_net_edit();
            // rời màn hình confirm về MENU chính để tránh kẹt ở "OK / EXIT"
            uiMode = UI_MODE_MENU;
          }
          else
          {
            // EXIT: bỏ thay đổi, THOÁT về MENU
            // netConfirming = false;
            // netEditTarget = nullptr;
            // lastNetCfgIndex = -1;
            // netEditDigitIndex = 0;
            lcdv2_reset_net_state();

            uiMode = UI_MODE_MENU;
            // lcd.clear();
            lcdv2_show_hint("MENU", "Turn to select");
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
          // lcd.clear();
          netConfigLastInteractionMs = millis();
          lcdv2_show_hint("MENU", "Turn to select");
        }
      }

      // RẤT QUAN TRỌNG: đánh dấu đã xử lý xong nhấn ngắn cho lần bấm này
      longPressHandled = true;
    }
  }

  // Cập nhật trạng thái cuối
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
    lcdv2_print_line(0, "MQTT VIEW");
    snprintf(buffer, sizeof(buffer), "Port:%d", mqtt_port);
    lcdv2_print_line(1, buffer);
    break;
  default:
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
    lcdv2_print_line(0, "MQTT PORT");
    snprintf(buffer, sizeof(buffer), "Port:%d", mqtt_port);
    lcdv2_print_line(1, buffer);
    break;
  }

  // --- ĐANG EDIT TỪNG CHỮ SỐ ---
  if (netEditing)
  {
    const char *titles[] = {"SET IP", "SET GW", "SET SUB", "SET DNS", "SET PORT"};
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

inline void lcdv2_show_hint(const char *line1, const char *line2)
{
  lcd.clear();
  lcdv2_print_line(0, line1);
  lcdv2_print_line(1, line2 ? line2 : "");
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
  persistNetworkConfig();

  // Đánh dấu telemetry cần gửi lại ngay lập tức để phản ánh IP mới
  telemetryDirty = true;
  nextPublishChannel = 0;

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

inline void lcdv2_reset_net_state()
{
  netEditing = false;
  netConfirming = false;
  netConfirmSelection = 0;
  netEditTarget = nullptr;
  netEditDigitIndex = 0;
  lastNetCfgIndex = -1;
  lcd.noCursor();
}

inline void lcdv2_timeout()
{
  if (uiMode == UI_MODE_MENU || uiMode == UI_MODE_NET_MENU || uiMode == UI_MODE_NET_IP_CONFIG)
  {
    netConfigLastInteractionMs = millis();
  }
}

inline void lcdv2_handle_net_timeout()
{
  if (uiMode == UI_MODE_MENU || uiMode == UI_MODE_NET_MENU || uiMode == UI_MODE_NET_IP_CONFIG)
  {
    if (netConfigLastInteractionMs > 0 && millis() - netConfigLastInteractionMs >= NET_CONFIG_TIMEOUT_MS)
    {
      const bool menuTimeout = uiMode == UI_MODE_MENU;
      uiMode = UI_MODE_VIEW;
      netConfigLastInteractionMs = 0;
      lcdv2_reset_net_state();
      lcdv2_show_hint(menuTimeout ? "MENU TIMEOUT" : "NET TIMEOUT", "back to view");
    }
  }
}
