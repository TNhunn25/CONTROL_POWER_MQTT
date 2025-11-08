#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "HC4052.h"
#include "Hshopvn_Pzem004t_V2.h"
LiquidCrystal_I2C lcd(0x27, 16, 2);
int currentStateCLK;
int lastStateCLK;
String currentDir = "";
unsigned long lastButtonPress = 0;
const int pinA = 47;
const int pinB = 48;

int encoderPos = 0;
int flag_encoderPos = 0;
int pinALast, aVal;
bool bCW;
unsigned long lastReadMs = 0;

#define MUX_A 28
#define MUX_B 27
HC4052 mux(MUX_A, MUX_B);

#define UART_PZEM Serial3
Hshopvn_Pzem004t_V2 pzem(&UART_PZEM);

typedef struct
{
  float V, I, kWh;
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
  out.kWh = 0;
}

// Hiển thị 1 kênh lên LCD
int last_channel = -1;
void lcd_show(int channel)
{
  if (channel < 0)
  {
    channel = 0;
  }
  if (channel > 3)
  {
    channel = 3;
  }

  if (lastDisplayValid[channel] && last_channel == channel)
  {
    if (lastDisplayValues[channel].V == ch[channel].V &&
        lastDisplayValues[channel].I == ch[channel].I &&
        lastDisplayValues[channel].kWh == ch[channel].kWh)
    {
      return;
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Channel ");
  lcd.print(channel);
  lcd.print(channel + 1);
  lcd.print(": ");
  lcd.print(ch[channel].V, 1);
  lcd.print("V ");

  lcd.setCursor(0, 1);
  lcd.print(ch[channel].I, 2);
  lcd.print("A ");
  lcd.print(ch[channel].kWh, 3);
  lcd.print("Kwh");

  last_channel = channel;
  lastDisplayValues[channel] = ch[channel];
  lastDisplayValid[channel] = true;
}
void checkpulse2()
{
  currentStateCLK = digitalRead(pinA);
  if (currentStateCLK != lastStateCLK && currentStateCLK == HIGH)
  {
    if (digitalRead(pinB) != currentStateCLK)
    {
      encoderPos = encoderPos + 1;
      currentDir = "CCW";
    }
    else
    {
      encoderPos = encoderPos - 1;
      currentDir = "CW";
    }

    Serial.print("Direction: ");
    Serial.print(currentDir);
    Serial.print(" | Counter: ");
    Serial.println(encoderPos);
  }
  switch (encoderPos)
  {
  case 0:
  case 1:
    flag_encoderPos = 0;
    break;
  case 2:
  case 3:
    flag_encoderPos = 1;
    break;
  case 4:
  case 5:
    flag_encoderPos = 2;
    break;
  case 6:
  case 7:
    flag_encoderPos = 3;
    break;
  default:
    if (encoderPos <= 0)
    {
      flag_encoderPos = 0;
      encoderPos = 0;
    }
    else if (encoderPos > 8)
    {
      flag_encoderPos = 3;
      encoderPos = 8;
    }
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
}
inline void lcdv2_begin()
{
  pinMode(pinA, INPUT);
  pinMode(pinB, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();

#ifndef LCDV2_EMBEDDED
  Serial.begin(115200);
  UART_PZEM.begin(9600);
  pzem.begin();
#endif
  pinALast = digitalRead(pinA);
  lastStateCLK = pinALast;
  last_channel = -1;
  lastStateCLK = pinALast;
  last_channel = -1;
  for (uint8_t i = 0; i < 4; ++i)
  {
    lastDisplayValid[i] = false;
    lastDisplayValues[i].V = 0;
    lastDisplayValues[i].I = 0;
    lastDisplayValues[i].kWh = 0;
  }
  lastReadMs = millis() + 1000;
}

inline void lcdv2_tick_display()
{
  checkpulse2(); // doc xung
  lcd_show(flag_encoderPos);
}

inline void lcdv2_tick_standalone()
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

// #ifndef LCDV2_EMBEDDED
// void setup()
// {
//   lcdv2_begin();
// }
// void loop()
// {
//   lcdv2_tick_standalone();
// }
// #endif