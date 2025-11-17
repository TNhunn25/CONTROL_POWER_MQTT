#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "HC4052.h"
#include "Hshopvn_Pzem004t_V2.h"
#include <KY040.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
int currentStateCLK;
int lastStateCLK;
String currentDir = "";
unsigned long lastButtonPress = 0;

#define CLK_PIN 47 // aka. A
#define DT_PIN 48  // aka. B

KY040 g_rotaryEncoder(CLK_PIN, DT_PIN);

// const int pinA = 47;
// const int pinB = 48;
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
bool autoModeEnabled;
int lastRelayStates[RELAY_COUNT] = {-1};
unsigned long lastDisplayUpdateMs = 0;
int relayStates[RELAY_COUNT] = {0};

#define UART_PZEM Serial3
Hshopvn_Pzem004t_V2 pzem(&UART_PZEM);

typedef struct
{
  float V, I, P;
} PZEMVAL;

PZEMVAL ch[4]; // lưu dữ liệu cho 4 kênh
PZEMVAL lastDisplayValues[4];
bool lastDisplayValid[4] = {false, false, false, false};
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

  if (lastDisplayValid[channel] &&
      last_channel == channel &&
      lastDisplayValues[channel].V == ch[channel].V &&
      lastDisplayValues[channel].I == ch[channel].I &&
      lastDisplayValues[channel].P == ch[channel].P)
  {
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("CH ");
  lcd.print(channel + 1);
  lcd.print(": ");

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

// void isr_encoderCLK()
// {
//   static unsigned long lastStepMs = 0;
//   const unsigned long DEBOUNCE_MS = 2; // 1–3 ms đủ cho encoder cơ

//   currentStateCLK = digitalRead(pinA);

//   if (millis() - lastStepMs >= DEBOUNCE_MS)
//   {
//     if (currentStateCLK != lastStateCLK && currentStateCLK == HIGH)
//     {
//       if (digitalRead(pinB) != currentStateCLK)
//       {
//         encoderPos++;
//         currentDir = "CCW";
//       }
//       else
//       {
//         encoderPos--;
//         currentDir = "CW";
//       }
//       lastStepMs = millis();
//     }
//   }

//   // QUAN TRỌNG:
//   lastStateCLK = currentStateCLK;
// }
// void checkpulse()
// {
//   long pos;
//   noInterrupts();
//   pos = encoderPos;
//   interrupts();
//   if (pos < 0)
//     pos = 0;
//   if (pos > 8)
//     pos = 8;
//   flag_encoderPos = pos / 2;
//   if (flag_encoderPos > 3)
//     flag_encoderPos = 3;

//   Serial.print("Direction: ");
//   Serial.print(currentDir);
//   Serial.print(" | Counter: ");
//   Serial.print(pos);
//   Serial.print(" | Channel: ");
//   Serial.println(flag_encoderPos);
// }
void checkpulse2()
{
  // static unsigned long lastStepMs = 0;
  // const unsigned long DEBOUNCE_MS = 100; // 1–3 ms đủ cho encoder cơ

  // currentStateCLK = digitalRead(pinA);

  // if (millis() - lastStepMs >= DEBOUNCE_MS)
  // {
  //   // if (pulseIn(pinA, 1)>100)
  //   // {
  //   //   if (digitalRead(pinB) != 1)
  //   //   {
  //   //     encoderPos++;
  //   //     currentDir = "CCW";
  //   //   }
  //   //   else if (digitalRead(pinB) != 0)
  //   //   {
  //   //     encoderPos--;
  //   //     currentDir = "CW";
  //   //   }
  //   currentStateCLK = digitalRead(pinA);
  //   if (currentStateCLK != lastStateCLK && currentStateCLK == HIGH)
  //   {
  //     if (digitalRead(pinB) != currentStateCLK)
  //     {
  //       encoderPos++;
  //       currentDir = "CCW";
  //     }
  //     else
  //     {
  //       encoderPos--;
  //       currentDir = "CW";
  //     }

  //     // Giới hạn & map 2 nấc/1 kênh → 0..3
  //     if (encoderPos < 0)
  //       encoderPos = 0;
  //     if (encoderPos > 8)
  //       encoderPos = 8;
  //     flag_encoderPos = encoderPos / 2;
  //     if (flag_encoderPos > 3)
  //       flag_encoderPos = 3;

  //     Serial.print("Direction: ");
  //     Serial.print(currentDir);
  //     Serial.print(" | Counter: ");
  //     Serial.println(encoderPos);
  //     Serial.print(" | Channel: ");
  //     Serial.println(flag_encoderPos);

  //     lastStepMs = millis();
  //     ui32_timeout_mqtt = millis() + 5000;
  //     timeout_readpzem = millis() + 500;
  //   }
  // }

  // // QUAN TRỌNG:
  // lastStateCLK = currentStateCLK;

  static int value = 0;
  // You have to run getRotation() very frequently in loop to prevent missing rotary encoder signals
  // If this is not possible take a look at the pinChangeInterrupt examples
  switch (g_rotaryEncoder.getRotation())
  {
  case KY040::CLOCKWISE:
    encoderPos++;

    if (encoderPos > 4)
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
      encoderPos = 4;
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

// lastStateCLK = currentStateCLK;
// lastReadMs = millis() + 500;
// int btnState = digitalRead(SW);
// if (btnState == LOW) {
//     if (millis() - lastButtonPress > 50) {

//             }
//     lastButtonPress = millis();
// }
// delay(1);
// }
inline void lcdv2_begin()
{
  // pinMode(pinA, INPUT);
  // pinMode(pinB, INPUT);

  // pinMode(pinA, INPUT_PULLUP);
  // pinMode(pinB, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  lcd.clear();

#ifndef LCDV2_EMBEDDED
  UART_PZEM.begin(9600);
  pzem.begin();
#endif
  pinALast = digitalRead(CLK_PIN);
  lastStateCLK = pinALast;
  last_channel = -1;
  lastStateCLK = pinALast;
  last_channel = -1;
  for (uint8_t i = 0; i < 4; ++i)
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
  if (flag_encoderPos < 4)
  {
    lcd_show(flag_encoderPos);
  }
  else if (flag_encoderPos = 4)
  {
    lcdshowchannelstate(flag_encoderPos);
  }
  // lcd_show(flag_encoderPos);
}

inline void lcdv2_tick_standalone() // đọc ch[0..3] mỗi 500ms
{
  if (lastReadMs <= millis())
  {
    for (uint8_t i = 0; i < 4; i++)
    {
      read_pzem_channel(i, ch[i]);
    }
    lastReadMs = millis() + 500;
  }
}
