/*
  Link product: 
  - https://hshop.vn/products/mach-do-ap-dong-cong-suat-nang-luong-ac-100a-giao-tiep-uart
  - https://hshop.vn/products/kit-rf-thu-phat-wifi-esp8266-nodemcu
  Wiring:

        NodeMCU-Esp8266-----------------------------PZEM004T
        Vcc-3.3V(only test) ---------------------------5V
        GND   ----------------------------------------GND
        D1(RX) <--------------<----------------------TX_PZEM
        D2(TX) --------------->--------------------->RX_PZEM

*/

#include "Hshopvn_Pzem004t_V2.h"

#define RX_PZEM     D2
#define TX_PZEM     D1

// Using with SoftwareSerial.
Hshopvn_Pzem004t_V2 pzem1(TX_PZEM, RX_PZEM);

// Using with HardwareSerial like: Serial,
// Hshopvn_Pzem004t_V2 pzem1(&Serial);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Start Hshop test PZEM004T V2 ! ++++");

  // init module
  pzem1.begin();
  pzem1.setTimeout(100);
  
  //Uncomment below if you want to reset Energy value
//  bool rr;
//  rr = pzem1.resetEnergy();
//  if(rr){
//    Serial.println("reset Energy ok");
//  }
  
}

void loop() {
  // put your main code here, to run repeatedly:

  //Get data from your PZEM004T V2
  pzem_info pzemData = pzem1.getData();

  Serial.print(pzemData.volt);
  Serial.print(F("V - "));
  Serial.print(pzemData.ampe);
  Serial.print(F("A - "));
  Serial.print(pzemData.power);
  Serial.print(F("W - "));
  Serial.print(pzemData.energy);
  Serial.print(F("Wh - "));
  Serial.print(pzemData.freq);
  Serial.print(F("Hz - "));
  Serial.print(pzemData.powerFactor);
  Serial.println();
  delay(1000);
  
}
