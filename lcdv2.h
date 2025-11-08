#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "HC4052.h"
#include "Hshopvn_Pzem004t_V2.h"
LiquidCrystal_I2C lcd(0x27, 16, 2);
int currentStateCLK;
int lastStateCLK;
String currentDir ="";
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

typedef struct {
  float V, I, P;
} PZEMVAL;

PZEMVAL ch[4];   // lưu dữ liệu cho 4 kênh
void read_pzem_channel(int channel, PZEMVAL &out) {
  mux.setChannel(channel);
  delay(40); // cho PZEM ổn định qua mux
  pzem_info d = pzem.getData();
  out.V = d.volt;
  out.I = d.ampe;
  out.P = d.power;
  // // Debug (tùy chọn)
  // Serial.print("CH"); Serial.print(channel);
  // Serial.print(" | V=");  Serial.print(out.V);
  // Serial.print(" I=");    Serial.print(out.I);
  // Serial.print(" P=");    Serial.println(out.P);
}

// Hiển thị 1 kênh lên LCD
int last_channel=0;
void lcd_show(int channel) {
   if(last_channel == channel)
   {
    return ;
   }
  if(last_channel !=channel)
  {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Channel"); lcd.print(channel); lcd.print(": ");
  lcd.print(ch[channel].V, 1); lcd.print("V ");

  lcd.setCursor(0, 1);
  lcd.print(ch[channel].I, 2); lcd.print("A ");
  lcd.print(ch[channel].P, 1); lcd.print("Kwh");
  last_channel = channel;
  }
}
void checkpulse2()
{
  currentStateCLK = digitalRead(pinA);
    if (currentStateCLK != lastStateCLK  && currentStateCLK == 1){
        if (digitalRead(pinB) != currentStateCLK) {
            encoderPos = encoderPos + 1;
            currentDir ="CCW";
        } else {
            encoderPos = encoderPos - 1 ;
            currentDir ="CW";
        }

        Serial.print("Direction: ");
        Serial.print(currentDir);
        Serial.print(" | Counter: ");
        Serial.println(encoderPos);
    }
  switch (encoderPos) {
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
    if (encoderPos <= 0) {
      flag_encoderPos = 0;
      encoderPos = 0;
    } 
    else if (encoderPos > 8) {
      flag_encoderPos = 3;
      encoderPos = 8;
    }
    break;
}

    lastStateCLK = currentStateCLK;
      lastReadMs = millis()+ 500; 
    // int btnState = digitalRead(SW);
    // if (btnState == LOW) {
    //     if (millis() - lastButtonPress > 50) {

    //             }
    //     lastButtonPress = millis();
    // }
    delay(1);
}
void setup() {
  pinMode(pinA, INPUT);
  pinMode(pinB, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  Serial.begin(115200);
  UART_PZEM.begin(9600);
  pzem.begin();
  pinALast = digitalRead(pinA);
  lastReadMs = millis()+ 1000; 
}
void loop() {
   checkpulse2(); // doc xung 
   lcd_show(flag_encoderPos);
  if (lastReadMs <= millis()) {
    for (uint8_t i = 0; i < 4; i++) {
      read_pzem_channel(i, ch[i]);
    }
    lastReadMs = millis()+500;
  }
}