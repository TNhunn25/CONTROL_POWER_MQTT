// mật khẩu là hivemq user name admin, http://192.168.11.48:8090/#!Clients
// http://mqtt.dev-altamedia.com
// http://debug-mqtt.dev-altamedia.com
// POWER/01/+

// http://192.168.0.50:8090/#!Clients
// http://192.168.0.50:8091/
// hot:192.168.0.50   //port:8000  //admin   //ALTA@2020
////////////////////////0////////////////
int timerketnoimang = 20000;
int timerketnoiserver = 0; // 10000;
int test = 1;
////////////////////////////////////////
#include "CRC16.h"
/////////////////////////////////////////
#include <string.h>
////////////////////////////////Ethernet
#include <Ethernet.h>
#include <PubSubClient.h>
#include "POWER_AUTO.h"
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

constexpr char DEVICE_ID[] = "POWER_CTRL_01";
unsigned long timer_ketnoimang = 0;
unsigned long timer_ketnoiserver = 0;

uint8_t mac[6] = {0x00, 0x01, 0x02, 0x04, 0x00, 0x00}; //******// phan biet lay dia chi ip
String ip = "";

IPAddress ipaddress(192, 168, 80, 141); //{192,168,10,254};                      //ip mang dang dung
IPAddress gateway(192, 168, 80, 254);   //{192,168,10,1};                   //truy cập internet qua router
IPAddress subnet(255, 255, 255, 0);     //{255,255,255,0};
IPAddress dns(8, 8, 8, 8);
int ketnoimang = 0;
/////////////////////////////MQTT
String NameString = "POWER"; //"POWER";

int ID = 0;
int IDtam = 0;
String IDString = "";

char *CLIENT_ID = "DEVICE1"; // SERVER phan biet board
String CLIENT_IDString = "";
char CLIENT_IDChar[50];

// #define mqtt_topic_sub_CONTROL "POWER/131/CONTROL"
char *mqtt_topic_sub_CONTROL = "";
String CONTROLString = "";
char CONTROLChar[50];

// #define mqtt_topic_sub_STATUS "POWER/02/STATUS"
char *mqtt_topic_sub_STATUS = "";
String STATUSString = "";
char STATUSChar[50];

// #define mqtt_topic_pub_REPLY "POWER/02/REPLY"
char *mqtt_topic_pub_REPLY = "";
String REPLYString = "";
char REPLYChar[50];

// #define mqtt_topic_pub_MODE "POWER/02/MODE"
char *mqtt_topic_pub_MODE = "";
String MODEString = "";
char MODEChar[50];

// #define mqtt_topic_pub_RELAY "POWER/02/RELAY"
char *mqtt_topic_pub_RELAY = "";
String RELAYString = "";
char RELAYChar[50];

// #define mqtt_topic_pub_V "POWER/02/V"
char *mqtt_topic_pub_V = "";
String VString = "";
char VChar[50];

// #define mqtt_topic_pub_A "POWER/02/A"
char *mqtt_topic_pub_A = "";
String AString = "";
char AChar[50];

// //#define mqtt_topic_pub_TEM "POWER/02/TEM"
// char* mqtt_topic_pub_TEM= "";
// String TEMString="";
// char TEMChar[50];

// //#define mqtt_topic_pub_HUM"POWER/02/HUM"
// char* mqtt_topic_pub_HUM= "";
// String HUMString="";
// char HUMChar[50];

// #define mqtt_topic_pub_AUTO"POWER/02/AUTO"
char *mqtt_topic_pub_AUTO = "";
String AUTOString = "";
char AUTOChar[50];

///////////////////////////////////////////////////////////////SERVER
// #define mqtt_server "192.168.11.48" // Thay bằng thông tin của bạn
char *mqtt_server = "mqtt.dev-altamedia.com"; //"mqtt://mqtt.dev-altamedia.com";//"192.168.11.48";
String serverString = "";
char serverChar[50];

// const uint16_t mqtt_port = 1884; //1884;//Port của CloudMQTT
int mqtt_port = 1883;
int port = 0;
byte portH = 0;
byte portL = 0;
#define mqtt_user "altamedia"
#define mqtt_pwd "Altamedia"

////////////////////////////////////////////////////////////////
#include <EEPROM.h>
int luuvaoEEPROM = 0;
//////////////////////////////DO DIEN AP DONG DIEN
#include "Hshopvn_Pzem004t_V2.h"

Hshopvn_Pzem004t_V2 pzemTong(&Serial2);

Hshopvn_Pzem004t_V2 pzem1234(&Serial3);
const int S1_pzem1234 = 27;
const int S0_pzem1234 = 28;

Hshopvn_Pzem004t_V2 pzem5678(&Serial1);
const int S1_pzem5678 = 29;
const int S0_pzem5678 = 39;

int dienap[9];
float dongdien[9];
float congsuat[9];
float diennangtieuthu[9];

long timer_docdienapdongdien = 0;
////////////////////////////Nhiet do, do am
#include <Adafruit_SHT31.h>
Adafruit_SHT31 Sensor;
int temp_c1;
int temp_f1;
int humidity1;
long timer_docnhietdodoam = 0;
//////////////////////////////LCD-I2c
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

int onoffdennen = 0;
long timer_lcd = 0;
int chupnhay = 0;
/////////////////////////////////////
#include <TimerOne.h> // khai báo thư viện timer 1
int ketnoiserver = 0;
//////////////////////////////Relay
byte Relay[] = {5, 6, 8, 11, 41, 37, 35, 33};
byte ReadRelay[] = {3, 7, 10, 12, 40, 36, 34, 32};
byte sorelay;

//////////////////////////////////UTC(time)
const uint8_t AUTO_OUTPUT_COUNT = sizeof(Relay) / sizeof(Relay[0]);
uint32_t autoStatusSeq[AUTO_OUTPUT_COUNT] = {0};
bool pzemMeasurementsUpdated = false;

unsigned long unixTimeBase = 0;
unsigned long unixTimeBaseMillis = 0;

unsigned long getUnixTimeUtc();

unsigned long getUnixTimeUtc()
{
  unsigned long now = millis();
  if (unixTimeBase == 0UL)
  {
    return now / 1000UL;
  }

  unsigned long elapsedSeconds = (now - unixTimeBaseMillis) / 1000UL;
  return unixTimeBase + elapsedSeconds;
}
////////////////////////////////////////

const char data_MQTT[100];

int n[25];
int n_1[25];

int sobytedata = 0;
int vitrirelay = 0;
int tatmo = 1;
int CRCLo = 0;
int CRCHi = 0;
int lenh = 0;

int mode = 0;
int guitraithairelay = 0;
int dienapgui[9];
int dongdiengui[9];
int temp_c1gui = 0;
int humidity1gui = 0;

int guitrangthaikhoidong = 1;
int khoidong = 1;
/////////////////////////////////////////////
int lanketnoiserver = 0;

const int Auto = 30; // doc trang thai auto man
const int Man = 31;
const int Den_Auto_Man = 13;
int den = 0;
//////////////////////////////////////////////////
const int A = 47;  // CLK
const int B = 48;  // DT
const int OK = 49; // SW

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
/////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void setup(void)
{
  Ethernet.init(53);
  Serial.begin(9600);
  Serial.println("POWER CONTROL  ARB PC-04");
  // Serial.print("ID: "); Serial.println(ID);
  // delay(1000);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(10);
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
  lcd.init();
  lcd.clear();
  lcd.backlight(); // lcd.noBacklight();
  lcd.setCursor(0, 0);
  lcd.print("POWER CONTROL");
  lcd.setCursor(0, 1);
  lcd.print("ARB PC-04");
  delay(1000);
  Ethernet.begin(mac, ipaddress, dns, gateway, subnet);
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());
  mqttClient.setServer(mqtt_server, mqtt_port);
  ///////////////////////////////////////////////////
  // doccauhinh();
  long now = millis();
  timer_ketnoimang = now + timerketnoimang;
  ConnectEthernet();
  ////////////////////////////////////////////////////
  /*
  lcd.clear();
  lcd.setCursor(0, 0);lcd.print("IS CONNECTING");
  lcd.setCursor(0, 1);lcd.print("TO THE NETWORK...");//CANNOT CONNECT TO THE NETWORK
  delay(1000);
  while(ketnoimang==0)
      {
        if(IPtinh==0)Ethernet.begin(mac);
        else Ethernet.begin(mac, ipaddress, gateway, subnet);
        Serial.print("IP address: ");
        Serial.println(Ethernet.localIP());
        ip = String (Ethernet.localIP()[0]);ip = ip + ".";ip = ip + String (Ethernet.localIP()[1]);ip = ip + ".";ip = ip + String (Ethernet.localIP()[2]);ip = ip + ".";ip = ip + String (Ethernet.localIP()[3]);
        //Serial.println(ip);
        if (ip=="0.0.0.0")ketnoimang=0;
        else ketnoimang=1;
        //Serial.println(ketnoimang);
      }

  lcd.clear();
  lcd.print(ip);
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);lcd.print("CONNECTING");
  lcd.setCursor(0, 1);lcd.print("TO SERVER...");

  mqttClient.setClient(ethClient);
  mqttClient.setServer(mqtt_server, mqtt_port);//use public broker
  mqttClient.setCallback(docenthernet);

  mqttClient.subscribe(mqtt_topic_sub_CONTROL, 1);// subscribe (topic, [qos])
  delay(1000);
  lcd.clear();
  */
  pzemTong.begin();
  pzemTong.setTimeout(100);
  // pzemTong.resetEnergy();

  pinMode(S1_pzem1234, OUTPUT);
  pinMode(S0_pzem1234, OUTPUT);
  pzem1234.begin();
  pzem1234.setTimeout(100);
  // digitalWrite(S1_pzem1234,0);digitalWrite(S0_pzem1234,0);pzem1234.resetEnergy();

  pinMode(S1_pzem5678, OUTPUT);
  pinMode(S0_pzem5678, OUTPUT);
  pzem5678.begin();
  pzem5678.setTimeout(100);
  timer_docdienapdongdien = (millis() - 10000);

  // Timer1.initialize(300000); //Microseconds
  // Timer1.attachInterrupt(ethernet); // khai báo ngắt timer 1

  pinMode(Auto, INPUT_PULLUP);
  pinMode(Man, INPUT_PULLUP);
  pinMode(Den_Auto_Man, OUTPUT);

  pinMode(A, INPUT);
  pinMode(B, INPUT);
  pinMode(OK, INPUT);
}
//////////////////////////////////////////////////////////////////////////////////////
void (*resetBoard)(void) = 0; // cài đặt hàm reset
///////////////////////////////////////////////////////////////////////////////////////
void doccauhinh()
{
  docEEPROM();
  Serial.print("ID: ");
  Serial.println(ID);

  IDString = ID;

  Serial.print("NAME: ");
  for (int i = 0; i < 5; i++)
  {
    Serial.print(chuChar[Name[i]]);
  }
  Serial.println("");
  for (int i = 0; i < 5; i++)
  {
    NameString += chuString[Name[i]];
  }
  NameString += "/";
  CLIENT_IDString = (NameString += IDString);
  CLIENT_IDString.toCharArray(CLIENT_IDChar, CLIENT_IDString.length() + 1);
  CLIENT_ID = CLIENT_IDChar;

  Serial.print("CLIENT_ID: ");
  Serial.println(CLIENT_ID);

  mac[4] = ID >> 8;
  mac[5] = ID & 0x00FF; // dua vao ID de lay dia chi Mac cho board
  // Serial.print("mac[]: ");for(int i=0;i<6;i++) {Serial.print(mac[i],HEX);Serial.print(" ");}Serial.println("");

  ///////////////////////////////////////////////////////SERVER
  // for(int i=0;i<32;i++){Serial.print(chuChar[ServerAddresstemp[i]]);}Serial.println("");
  for (int i = 0; i < 32; i++)
  {
    if (ServerAddress[i] == 0)
      i = 32;
    else
      serverString += chuString[ServerAddress[i]];
  }
  serverString.toCharArray(serverChar, serverString.length() + 1);
  mqtt_server = "mqtt.dev-altamedia.com";
  Serial.print("SERVER ADDRESS: ");
  Serial.println(mqtt_server);
  //////////////////////////////////////////////////////PORT
  port = (portH << 8) + (portL); // Serial.print("port: ");Serial.println(port);
  mqtt_port = 1883;
  Serial.print("mqtt_port: ");
  Serial.println(mqtt_port);

  /////////////////////////////////////////////////////
  CONTROLString = CLIENT_IDString;
  CONTROLString += "/CONTROL";
  CONTROLString.toCharArray(CONTROLChar, CONTROLString.length() + 1);
  mqtt_topic_sub_CONTROL = CONTROLChar;
  Serial.print("mqtt_topic_sub_CONTROL: ");
  Serial.println(mqtt_topic_sub_CONTROL);

  STATUSString = CLIENT_IDString;
  STATUSString += "/STATUS";
  STATUSString.toCharArray(STATUSChar, STATUSString.length() + 1);
  mqtt_topic_sub_STATUS = STATUSChar;
  Serial.print("mqtt_topic_sub_STATUS: ");
  Serial.println(mqtt_topic_sub_STATUS);

  // char* mqtt_topic_pub_MODE= "POWER/02/MODE"
  MODEString = CLIENT_IDString;
  MODEString += "/MODE";
  MODEString.toCharArray(MODEChar, MODEString.length() + 1);
  mqtt_topic_pub_MODE = MODEChar;
  Serial.print("mqtt_topic_sub_MODE: ");
  Serial.println(mqtt_topic_pub_MODE);

  // char* mqtt_topic_pub_RELAY= "POWER/02/RELAY"
  RELAYString = CLIENT_IDString;
  RELAYString += "/RELAY";
  RELAYString.toCharArray(RELAYChar, RELAYString.length() + 1);
  mqtt_topic_pub_RELAY = RELAYChar;
  Serial.print("mqtt_topic_sub_RELAY: ");
  Serial.println(mqtt_topic_pub_RELAY);

  // char* mqtt_topic_pub_V= "POWER/02/V"
  VString = CLIENT_IDString;
  VString += "/V";
  VString.toCharArray(VChar, VString.length() + 1);
  mqtt_topic_pub_V = VChar;
  Serial.print("mqtt_topic_sub_V: ");
  Serial.println(mqtt_topic_pub_V);

  // char* mqtt_topic_pub_A= "POWER/02/A"
  AString = CLIENT_IDString;
  AString += "/A";
  AString.toCharArray(AChar, AString.length() + 1);
  mqtt_topic_pub_A = AChar;
  Serial.print("mqtt_topic_sub_A: ");
  Serial.println(mqtt_topic_pub_A);

  // // char* mqtt_topic_pub_TEM= "POWER/02/TEM"
  // TEMString = CLIENT_IDString;
  // TEMString += "/TEM";
  // TEMString.toCharArray(TEMChar, TEMString.length() + 1);
  // mqtt_topic_pub_TEM = TEMChar;
  // Serial.print("mqtt_topic_sub_TEM: ");
  // Serial.println(mqtt_topic_pub_TEM);

  // // char* mqtt_topic_pub_HUM="POWER/02/HUM"
  // HUMString = CLIENT_IDString;
  // HUMString += "/HUM";
  // HUMString.toCharArray(HUMChar, HUMString.length() + 1);
  // mqtt_topic_pub_HUM = HUMChar;
  // Serial.print("mqtt_topic_sub_HUM: ");
  // Serial.println(mqtt_topic_pub_HUM);

  // char* mqtt_topic_pub_AUTO="POWER/02/AUTO"
  AUTOString = CLIENT_IDString;
  AUTOString += "/AUTO";
  AUTOString.toCharArray(AUTOChar, AUTOString.length() + 1);
  mqtt_topic_pub_AUTO = AUTOChar;
  Serial.print("mqtt_topic_pub_AUTO: ");
  Serial.println(mqtt_topic_pub_AUTO);
}
///////////////////////////////////////////////////////////////////////////////////////
void ConnectEthernet()
{
  long now = millis();
  if (now - timer_ketnoimang > timerketnoimang)
  {
    timer_ketnoimang = now;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IS CONNECTING");
    lcd.setCursor(0, 1);
    lcd.print("TO THE NETWORK..."); // CANNOT CONNECT TO THE NETWORK
    delay(1000);
    // if (IPtinh == 0)
    //   Ethernet.begin(mac);
    // else
    mqttClient.setServer(mqtt_server, mqtt_port); // use public broker
    mqttClient.setCallback(docenthernet);
    Ethernet.begin(mac, ipaddress, dns, gateway, subnet);
    Serial.print("IP address: ");
    Serial.println(Ethernet.localIP());
    ip = String(Ethernet.localIP()[0]);
    ip = ip + ".";
    ip = ip + String(Ethernet.localIP()[1]);
    ip = ip + ".";
    ip = ip + String(Ethernet.localIP()[2]);
    ip = ip + ".";
    ip = ip + String(Ethernet.localIP()[3]);
    // Serial.println(ip);
    if (ip == "0.0.0.0")
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("NOT CONNECTED");
      delay(1000);
      lcd.clear();
      ketnoimang = 0;
    }
    else
    {
      lcd.clear();
      if (IPtinh == 0)
      {
        lcd.setCursor(0, 0);
        lcd.print("CONNECTED");
        lcd.setCursor(0, 1);
        lcd.print(ip);
      }
      else
      {
        lcd.print(ip);
      }
      ketnoimang = 1;
      // lcd.clear();
      // lcd.setCursor(0, 0);lcd.print("CONNECTING");
      // lcd.setCursor(0, 1);lcd.print("TO SERVER...");
      // delay(1000);
      // mqttClient.setClient(ethClient);
      // mqttClient.setServer(mqtt_server, mqtt_port); // use public broker
      // mqttClient.setCallback(docenthernet);

      mqttClient.subscribe(mqtt_topic_sub_CONTROL, 1); // subscribe (topic, [qos])

      delay(1000);
      lcd.clear();
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////
void ethernet()
{
  // Serial.println("1");
  if (ketnoiserver == 1)
  {
    mqttClient.loop();

    lanketnoiserver++;
    if (lanketnoiserver == 33)
    {
      // guitrangthai();
      // gui tu dong
      // snprintf (guiSERVER, 50, "ID: %d", ID);
      // mqttClient.publish(mqtt_topic_pub_RELAY,guiSERVER);
      lanketnoiserver = 0;
    }
  }
  else
  {
  }
}
void reconnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("arduinoClient", "altamedia", "Altamedia"))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe("inTopic");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////
void reconnectserver()
{
  long now = millis();
  if (now - timer_ketnoiserver >= timerketnoiserver) //
  {
    timer_ketnoiserver = now;

    // Ethernet.localIP();

    if (mqttClient.connect(CLIENT_ID, mqtt_user, mqtt_pwd)) //;mqttClient.connect(CLIENT_ID,mqtt_user, mqtt_pwd)) // Thực hiện kết nối với mqtt user và pass
                                                            // connect(CLIENT_ID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])

    {
      Serial.println("connected");
      // byte datasend[]={0xFF,0xFF,0xFF,0xFF};
      byte datasend[] = {0x31};
      mqttClient.publish(mqtt_topic_sub_STATUS, datasend, 1, 1); // Khi kết nối sẽ publish thông báo
      mqttClient.subscribe(mqtt_topic_sub_CONTROL, 1);           // subscribe (topic, [qos])   // ... và nhận lại thông tin này

      lcd.backlight();
      ketnoiserver = 1;
      guitrangthaikhoidong = 1;
      khoidong = 1;
    }
    else
    {
      // onoffdennen=not(onoffdennen);
      Serial.println("connected failed"); // delay(2000);
      lcd.noBacklight();
      delay(500);
      /*
      ketnoimang++;//kiem ta mat ket noi sever co do mat mang
      if(ketnoimang==2||ketnoimang%5==0)
        {
          if (Ethernet.begin(mac) == 0) {}
          //Serial.print("IP address: ");
          Ethernet.localIP();
          ip = String (Ethernet.localIP()[0]);ip = ip + ".";ip = ip + String (Ethernet.localIP()[1]);ip = ip + ".";ip = ip + String (Ethernet.localIP()[2]);ip = ip + ".";ip = ip + String (Ethernet.localIP()[3]);
          //Serial.println(ip);
          if (ip=="0.0.0.0");
          else ketnoimang=1;
          //Serial.println(ketnoimang);
        }*/
    }
    lcd.backlight();
  }
}
///////////////////////////////////////////////////////////////////////////////////////
void docenthernet(char *topic, byte *payload, unsigned int length)
{
  Serial.print("[");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++)
  {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  char crcdata[7];
  vitrirelay = int(payload[0]);
  crcdata[0] = vitrirelay;
  Serial.print("vitrirelay:");
  Serial.print(vitrirelay);
  Serial.print(" ");
  tatmo = int(payload[1]);
  crcdata[1] = tatmo;
  Serial.print("tatmo:");
  Serial.print(tatmo);
  Serial.print(" ");
  CRCLo = int(payload[2]);
  Serial.print("CRCLo:");
  Serial.print(CRCLo);
  Serial.print(" ");
  CRCHi = int(payload[3]);
  Serial.print("CRCHi:");
  Serial.println(CRCHi);
  unsigned int codeCRC = calcCRC16(2, crcdata);
  // Serial.print(codeCRC >> 8);Serial.print(" ");Serial.println(codeCRC & 0x00FF);
  if ((vitrirelay <= 20) && (tatmo == 0 || tatmo == 255) && (CRCLo == (codeCRC >> 8)) && (CRCHi == (codeCRC & 0x00FF)))
  {
    byte datasend[] = {vitrirelay, tatmo, CRCLo, CRCHi};
    mqttClient.publish(mqtt_topic_pub_REPLY, datasend, 4, 1);

    if (tatmo == 255)
    {
      tatmo = 1;
    }
    else
    {
      tatmo = 0;
    }
    lenh = 1;
  }
  else
  {
    byte datasend[] = {0x00, 0x00, 0x00, 0x00};
    mqttClient.publish(mqtt_topic_pub_REPLY, datasend, 4, 1);
  }
}
////////////////////////////////////////////////////////////////////////////////////////
void dieukhienrelay()
{
  // Serial.println("4");
  if (vitrirelay != 0 && vitrirelay <= sorelay)
  {
    digitalWrite(Relay[vitrirelay - 1], not(tatmo));
  }
  else if (vitrirelay == 0)
  {
    test = not(test);
    for (int i = 0; i < sorelay; i++)
    {
      digitalWrite(Relay[i], test); // not(tatmo)
      // delay(1000);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////
void docdienapdongdien()
{
  long now = millis();
  if (now - timer_docdienapdongdien > 10000)
  {
    timer_docdienapdongdien = now;

    pzem_info pzemData;
    for (int i = 0; i < 9; i++)
    {
      if (i == 0)
      {
        pzemData = pzemTong.getData();
      }
      else if (i >= 1 && i <= 4)
      {
        if (i == 1)
        {
          digitalWrite(S1_pzem1234, 0);
          digitalWrite(S0_pzem1234, 0);
        }
        else if (i == 2)
        {
          digitalWrite(S1_pzem1234, 0);
          digitalWrite(S0_pzem1234, 1);
        }
        else if (i == 3)
        {
          digitalWrite(S1_pzem1234, 1);
          digitalWrite(S0_pzem1234, 0);
        }
        else if (i == 4)
        {
          digitalWrite(S1_pzem1234, 1);
          digitalWrite(S0_pzem1234, 1);
        }
        pzemData = pzem1234.getData();
      }
      else if (i >= 5 && i <= 8)
      {
        if (i == 5)
        {
          digitalWrite(S1_pzem5678, 0);
          digitalWrite(S0_pzem5678, 0);
        }
        else if (i == 6)
        {
          digitalWrite(S1_pzem5678, 0);
          digitalWrite(S0_pzem5678, 1);
        }
        else if (i == 7)
        {
          digitalWrite(S1_pzem5678, 1);
          digitalWrite(S0_pzem5678, 0);
        }
        else if (i == 8)
        {
          digitalWrite(S1_pzem5678, 1);
          digitalWrite(S0_pzem5678, 1);
        }
        pzemData = pzem5678.getData();
      }

      // pzem_info pzemData = pzemTong.getData();
      if ((pzemData.volt >= 0) && (pzemData.volt < 260))
      {
        dienap[i] = pzemData.volt;
      }
      if (pzemData.ampe >= 0)
      {
        dongdien[i] = pzemData.ampe;
      }
      if (pzemData.power >= 0)
      {
        congsuat[i] = pzemData.power;
      }
      if (pzemData.energy >= 0)
      {
        diennangtieuthu[i] = pzemData.energy;
      }
      // Serial.print(i);Serial.print(":  ");Serial.print(dienap[i]);Serial.print("V   ");Serial.print(dongdien[i]);Serial.print("A   ");
      // Serial.print(congsuat[i]);Serial.print("W   ");Serial.print(diennangtieuthu[i]);Serial.println("Wh   ");
    }
    pzemMeasurementsUpdated = true;
    // Serial.println("");
  }
}
////////////////////////////////////////////////////////////////////////////////////////
void docnhietdodoam()
{
  // Serial.println("6");
  long now = millis();
  if (now - timer_docnhietdodoam > 5000)
  {
    timer_docnhietdodoam = now;

    // Sensor.UpdateData();

    // temp_c1 = Sensor.GetTemperature();
    temp_c1 = Sensor.readTemperature();
    // temp_f1 = sht1x1.readTemperatureF();
    //  humidity1 = Sensor.GetRelHumidity();
    humidity1 = Sensor.readHumidity();
    // delay(2500);
  }
}

//////////////////////////////////////////////////////////////////
bool isAutoHardwareSelected()
{
  return (digitalRead(Auto) == HIGH && digitalRead(Man) == LOW);
}
/////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void hienLCD()
{
  int dongdienle = (dongdien[0] - (int(dongdien[0]))) * 10;
  if (dongdien[0] >= 10)
  {
    sprintf(temp, "%3dV %3dA", dienap[0], int(dongdien[0]));
  }
  else
  {
    sprintf(temp, "%3dV  %3d.%dA", dienap[0], int(dongdien[0]), dongdienle);
  }
  lcd.setCursor(0, 0);
  lcd.print(temp);

  lcd.setCursor(0, 1);
  lcd.print(temp_c1);
  lcd.write(223);
  lcd.print("C");
  if (temp_c1 < 10)
    lcd.print("   ");
  else if (temp_c1 < 100)
    lcd.print("  ");
  else
    lcd.print(" ");
  lcd.setCursor(6, 1);
  lcd.print(humidity1);
  lcd.print("%");
  if (humidity1 < 10)
    lcd.print("   ");
  else if (humidity1 < 100)
    lcd.print("  ");
  else
    lcd.print(" ");

  lcd.setCursor(12, 1);
  long now = millis();
  if (now - timer_lcd > 500)
  {
    timer_lcd = now;
    chupnhay = not chupnhay;
  }
  if (chupnhay == 0)
  {
    bool autoModeActive = isAutoHardwareSelected();
    if (!autoModeActive)
    {
      lcd.print("MAN ");
    }
    else
    {
      lcd.print("AUTO");
    }
  }
  else
    lcd.print("    ");
}

///////////////////////////////////////////////////////////////////////////////////////
void doctrangthai()
{
  // Serial.println("8");
  if (digitalRead(Man) == 1 && digitalRead(Auto) == 0)
  {
    digitalWrite(Den_Auto_Man, 1);
  }
  else if (digitalRead(Man) == 0 && digitalRead(Auto) == 1)
  {
    den = not(den);
    digitalWrite(Den_Auto_Man, den);
  }
  else
  {
    digitalWrite(Den_Auto_Man, 0);
  }

  docnhietdodoam(); // bi delay(2300)
  for (int i = 0; i < 8; i++)
  {
    n[i] = digitalRead(ReadRelay[i]);
    if (n[i] != n_1[i])
    {
      timer_docdienapdongdien = (millis() - 7500);
    }
    // Serial.print(n[i]);
  }
  // Serial.println("");
  docdienapdongdien();
}
////////////////////////////////////////////////////////////////////////////////////////
void guitrangthai()
{
  // Serial.println("9");
  bool modeChangedForAuto = false;
  bool relayChangedForAuto = false;
  bool measurementChangedForAuto = false;

  if (pzemMeasurementsUpdated)
  {
    measurementChangedForAuto = true;
    pzemMeasurementsUpdated = false;
  }
  //////////////////////////////////////////////////////////////MODE
  int mode1 = digitalRead(Auto);
  if ((mode != mode1) || guitrangthaikhoidong == 1)
  {
    // Serial.println("9.1");
    mode = mode1;

    byte modegui = 0x00;
    if (mode == 0)
      modegui = 0x00;
    else
      modegui = 0xFF;
    char crcdatagui[1];
    crcdatagui[0] = modegui;
    unsigned int codeCRCgui = calcCRC16(1, crcdatagui);
    int codeCRCguiLo = codeCRCgui >> 8;
    int codeCRCguiHi = codeCRCgui & 0x00FF;
    byte datasend[] = {modegui, codeCRCguiLo, codeCRCguiHi};
    mqttClient.publish(mqtt_topic_pub_MODE, data_MQTT);
  }

  bool isAutoModeActive = isAutoHardwareSelected();
  int mode2 = isAutoModeActive ? HIGH : LOW;
  if ((mode != mode2) || guitrangthaikhoidong == 1)
  {
    // Serial.println("9.1");
    mode = mode2;
    modeChangedForAuto = true;

    if (mode2 != 0)
    {
      memset(autoStatusSeq, 0, sizeof(autoStatusSeq));
    }
  }

  //////////////////////////////////////////////////////////////RELAY
  bool relayStateChanged = false;
  for (int a = 0; a < 20; a++)
  {
    if (n[a] != n_1[a])
    {
      n_1[a] = n[a];
      guitraithairelay = 1;
    }
  }
  if (guitraithairelay == 1)
  {
    relayStateChanged = true;
  }
  if (guitraithairelay == 1 || guitrangthaikhoidong == 1)
  {
    // Manual mode no longer publishes relay state over MQTT to avoid
    // collisions with AUTO JSON packets.
    guitraithairelay = 0;
  }
  if (relayStateChanged)
  {
    relayChangedForAuto = true;
  }
  //////////////////////////////////////////////////////////////DIEN AP
  // if (dienap[0] != dienapgui[0] || guitrangthaikhoidong == 1)
  // {
  //   // Serial.println("9.3");
  //   dienapgui[0] = dienap[0];
  //   char crcdatagui[2];
  //   crcdatagui[0] = dienap[0] >> 8;
  //   crcdatagui[1] = dienap[0] & 0x00FF;
  //   unsigned int codeCRCgui = calcCRC16(2, crcdatagui);
  //   int codeCRCguiLo = codeCRCgui >> 8;
  //   int codeCRCguiHi = codeCRCgui & 0x00FF;
  //   byte datasend[] = {dienap[0] >> 8, dienap[0] & 0x00FF, codeCRCguiLo, codeCRCguiHi};
  //   mqttClient.publish(mqtt_topic_pub_V, datasend, 4, 1);
  // }

  if (dienap[0] != dienapgui[0] || guitrangthaikhoidong == 1)
  {
    // Serial.println("9.3");
    dienapgui[0] = dienap[0];
    // Manual mode skips voltage MQTT telemetry.
  }

  //////////////////////////////////////////////////////////////DONG DIEN
  const int dongdiensonguyen = dongdien[0];
  // if (dongdiensonguyen != dongdiengui[0] || guitrangthaikhoidong == 1)
  // {
  //   // Serial.println("9.6");
  //   dongdiengui[0] = dongdiensonguyen;
  //   char crcdatagui[1];
  //   crcdatagui[0] = dongdiensonguyen;
  //   unsigned int codeCRCgui = calcCRC16(1, crcdatagui);
  //   int codeCRCguiLo = codeCRCgui >> 8;
  //   int codeCRCguiHi = codeCRCgui & 0x00FF;
  //   byte datasend[] = {dongdiensonguyen, codeCRCguiLo, codeCRCguiHi};
  //   mqttClient.publish(mqtt_topic_pub_A, datasend, 3, 1);
  // }

  if (dongdiensonguyen != dongdiengui[0] || guitrangthaikhoidong == 1)
  {
    // Serial.println("9.6");
    dongdiengui[0] = dongdiensonguyen;
    // Manual mode skips current MQTT telemetry.
  }
  bool shouldPublishAutoJson = isAutoModeActive &&
                               (modeChangedForAuto || relayChangedForAuto || measurementChangedForAuto ||
                                (guitrangthaikhoidong == 1));

  if (shouldPublishAutoJson)
  {
    const size_t measurementCount = sizeof(dienap) / sizeof(dienap[0]);
    bool published = powerJsonPublishAutoStatus(mqttClient,
                                                mqtt_topic_pub_AUTO,
                                                CLIENT_ID,
                                                n,
                                                AUTO_OUTPUT_COUNT,
                                                dienap,
                                                measurementCount,
                                                dongdien,
                                                diennangtieuthu,
                                                autoStatusSeq,
                                                millis,
                                                isAutoModeActive);
    if (published)
    {
      guitrangthaikhoidong = 0;
    }
  }
}
//////////////////////////////////////////////////////////////NHIET DO
// if (temp_c1 != temp_c1gui || guitrangthaikhoidong == 1)
// {
//   // Serial.println("9.7");
//   temp_c1gui = temp_c1;
//   char crcdatagui[1];
//   crcdatagui[0] = temp_c1;
//   unsigned int codeCRCgui = calcCRC16(1, crcdatagui);
//   int codeCRCguiLo = codeCRCgui >> 8;
//   int codeCRCguiHi = codeCRCgui & 0x00FF;
//   byte datasend[] = {temp_c1, codeCRCguiLo, codeCRCguiHi};
//   mqttClient.publish(mqtt_topic_pub_TEM, datasend, 3, 1);

// } // humidity1

//////////////////////////////////////////////////////////////DO AM
//   if (humidity1 != humidity1gui || guitrangthaikhoidong == 1)
//   {
//     // Serial.println("9.8");
//     humidity1gui = humidity1;
//     char crcdatagui[1];
//     crcdatagui[0] = humidity1;
//     unsigned int codeCRCgui = calcCRC16(1, crcdatagui);
//     int codeCRCguiLo = codeCRCgui >> 8;
//     int codeCRCguiHi = codeCRCgui & 0x00FF;
//     byte datasend[] = {humidity1, codeCRCguiLo, codeCRCguiHi};
//     mqttClient.publish(mqtt_topic_pub_HUM, datasend, 3, 1);
//   }
//   guitrangthaikhoidong = 0;
// }
///////////////////////////////////////////////////////////////////////////////////////////
void docaserial()
{
  if (Serial.available() > 0)
  {
    vitrirelay = Serial.parseInt();
    tatmo = Serial.parseInt();
    Serial.read();
    lenh = 1;
    Serial.println("Serial");
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
void SetupLCD()
{
  long now = millis();
  timer_ketnoimang = now;
  timer_ketnoiserver = now;
  // if(nhanSetup==0){docencoder();}
  docencoder();
  if (Setup_1 > 0 && Setup_2 == 0 && Setup_3 == 0 && Setup_4 == 0) // lop 1
  {
    if (nhanSetup == 0)
    {
      if (giatriEncoder <= 0)
      {
        giatriEncoder = 5;
        dongchop = 1;
      }
      else if (giatriEncoder > 5)
      {
        giatriEncoder = 1;
        dongchop = 0;
      }
      Setup_1 = giatriEncoder;
    }
    else if (nhanSetup == 1)
    {
      lcd.clear();
      if (Setup_1 == 1)
      {
        Setup_2 = 1;
      } // vaoxem thong so
      else if (Setup_1 == 2)
      {
        Setup_2 = 1;
      }
      else if (Setup_1 == 3)
      {
        Setup_2 = 1;
      } // vao setup
      else if (Setup_1 == 4)
      {
        Setup_2 = 1;
      } // vao cai dat ngon ngu
      else if (Setup_1 == 5)
      {
        Setup_1 = 0;
      } // thoat
      dongchop = 0;
      giatriEncoder = 1;
      nhanSetup = 2;
    }
    choptat();
    if (chopchu > 0)
    {
      SetupLCD_0();
    }
  }
  //////////////////////////////////////////
  else if (Setup_1 > 0 && Setup_2 > 0 && Setup_3 == 0 && Setup_4 == 0) // lop 2
  {
    if (Setup_1 == 1) // thong so ouput
    {
      if (nhanSetup == 0)
      {
        if (giatriEncoder <= 0)
        {
          giatriEncoder = 11;
        }
        else if (giatriEncoder >= 12)
        {
          giatriEncoder = 1;
        }
        Setup_2 = giatriEncoder;
      }
      else if (nhanSetup == 1)
      {
        if (Setup_2 == 10) // vao reset out
        {
          Setup_3 = 1;
          dongchop = 0;
          giatriEncoder = 1;
        }
        else if (Setup_2 == 11)
          Setup_2 = 0; // thoat

        nhanSetup = 2;
      }
      SetupLCD_1();
    }
    else if (Setup_1 == 2) // parameter
    {
      if (nhanSetup == 0)
      {
        if (IPtinh == 0)
        {
          if (giatriEncoder <= 0)
          {
            giatriEncoder = 8;
          }
          else if (giatriEncoder > 8)
          {
            giatriEncoder = 1;
          }
        }
        else
        {
          if (giatriEncoder <= 0)
          {
            giatriEncoder = 11;
          }
          else if (giatriEncoder > 11)
          {
            giatriEncoder = 1;
          }
        }
        Setup_2 = giatriEncoder;
      }
      else if (nhanSetup == 1)
      {
        if (IPtinh == 0)
        {
          // if(Setup_2==8)Setup_2=0;//thoat
        }
        else
        {
          // if(Setup_2==8)Setup_2=0;//thoat
        }
        nhanSetup = 2;
      }
      // SetupLCD_2();
    }
    else if (Setup_1 == 3) // setup
    {
      if (nhanSetup == 0)
      {
        if (giatriEncoder <= 0)
        {
          giatriEncoder = 5;
          dongchop = 1;
        }
        else if (giatriEncoder >= 6)
        {
          giatriEncoder = 1;
          dongchop = 0;
        }
        Setup_2 = giatriEncoder;
      }
      else if (nhanSetup == 1)
      {
        if (Setup_2 == 1) // vao setup NAME
        {
          Setup_3 = 1;
          giatriEncoder = 1;
        }
        else if (Setup_2 == 2) // vao setup ID
        {
          Setup_3 = 1;
          giatriEncoder = ID;
          IDtam = ID;
        }
        else if (Setup_2 == 3) // vao setup IP
        {
          Setup_3 = IPtinh + 1;
          giatriEncoder = Setup_3;
        }
        else if (Setup_2 == 4) // vao setup SERVER
        {
          Setup_3 = 1;
          giatriEncoder = Setup_3;
          dongchop = 0;
        }
        else if (Setup_2 == 5)
        {
          Setup_2 = 0;
          giatriEncoder = Setup_1;
        } // Thoat

        nhanSetup = 2;
      }
      choptat();
      if (chopchu > 0)
      {
        SetupLCD_3();
      }
    }
    else if (Setup_1 == 4) // ngon ngu
    {
      choptat();
      if (chopchu > 0)
      {
        SetupLCD_4();
      }
      if (nhanSetup == 1)
      {
        if (dongchop == 0)
        {
          ngonngu = 0;
        }
        else
        {
          ngonngu = 1;
        }
        Setup_2 = 0;
        if (ngonngu != EEPROM.read(100))
        {
          EEPROM.write(100, ngonngu);
        }
        nhanSetup = 2;
      }
    }
  }
  /////////////////////////////////////////////////////
  else if (Setup_1 > 0 && Setup_2 > 0 && Setup_3 > 0 && Setup_4 == 0) // lop 3
  {
    if (Setup_1 == 1 && Setup_2 == 10) // reset out put
    {
      SetupLCD_1_10();
      if (nhanSetup == 0)
      {
        if (giatriEncoder <= 0)
        {
          giatriEncoder = 11;
          dongchop = 1;
        }
        else if (giatriEncoder >= 12)
        {
          giatriEncoder = 1;
        }
        Setup_3 = giatriEncoder;
      }
      else if (nhanSetup == 1)
      {
        if (Setup_3 == 1)
        {
          giatriEncoder = 10;
          Serial.println("reset het");
          pzemTong.resetEnergy();
          for (int i = 0; i < 9; i++)
          {
            if (i == 0)
            {
              pzemTong.resetEnergy();
            }
            else if (i >= 1 && i <= 4)
            {
              if (i == 1)
              {
                digitalWrite(S1_pzem1234, 0);
                digitalWrite(S0_pzem1234, 0);
              }
              else if (i == 2)
              {
                digitalWrite(S1_pzem1234, 0);
                digitalWrite(S0_pzem1234, 1);
              }
              else if (i == 3)
              {
                digitalWrite(S1_pzem1234, 1);
                digitalWrite(S0_pzem1234, 0);
              }
              else if (i == 4)
              {
                digitalWrite(S1_pzem1234, 1);
                digitalWrite(S0_pzem1234, 1);
              }
              pzem1234.resetEnergy();
            }
            else if (i >= 5 && i <= 8)
            {
              if (i == 5)
              {
                digitalWrite(S1_pzem5678, 0);
                digitalWrite(S0_pzem5678, 0);
              }
              else if (i == 6)
              {
                digitalWrite(S1_pzem5678, 0);
                digitalWrite(S0_pzem5678, 1);
              }
              else if (i == 7)
              {
                digitalWrite(S1_pzem5678, 1);
                digitalWrite(S0_pzem5678, 0);
              }
              else if (i == 8)
              {
                digitalWrite(S1_pzem5678, 1);
                digitalWrite(S0_pzem5678, 1);
              }
              pzem5678.resetEnergy();
            }
          }
        }
        else if (Setup_3 == 2)
        {
          giatriEncoder = 1;
          pzemTong.resetEnergy();
        }
        else if (Setup_3 == 11)
        {
          giatriEncoder = 10;
        }
        else
        {
          giatriEncoder = Setup_3 - 1;
          int i = Setup_3 - 2;
          if (i >= 1 && i <= 4)
          {
            if (i == 1)
            {
              digitalWrite(S1_pzem1234, 0);
              digitalWrite(S0_pzem1234, 0);
            }
            else if (i == 2)
            {
              digitalWrite(S1_pzem1234, 0);
              digitalWrite(S0_pzem1234, 1);
            }
            else if (i == 3)
            {
              digitalWrite(S1_pzem1234, 1);
              digitalWrite(S0_pzem1234, 0);
            }
            else if (i == 4)
            {
              digitalWrite(S1_pzem1234, 1);
              digitalWrite(S0_pzem1234, 1);
            }
            pzem1234.resetEnergy();
          }
          else if (i >= 5 && i <= 8)
          {
            if (i == 5)
            {
              digitalWrite(S1_pzem5678, 0);
              digitalWrite(S0_pzem5678, 0);
            }
            else if (i == 6)
            {
              digitalWrite(S1_pzem5678, 0);
              digitalWrite(S0_pzem5678, 1);
            }
            else if (i == 7)
            {
              digitalWrite(S1_pzem5678, 1);
              digitalWrite(S0_pzem5678, 0);
            }
            else if (i == 8)
            {
              digitalWrite(S1_pzem5678, 1);
              digitalWrite(S0_pzem5678, 1);
            }
            pzem5678.resetEnergy();
          }
        }

        Setup_3 = 0;
        nhanSetup = 2;
      }
    }
    else if (Setup_1 == 3 && Setup_2 == 1) // vao setup NAME
    {
      if (nhanSetup == 0)
      {
        if (setchu == 0) // khongsetchu
        {
          if (giatriEncoder <= 0)
          {
            giatriEncoder = 6;
          }
          else if (giatriEncoder >= 7)
          {
            giatriEncoder = 1;
          }
          Setup_3 = giatriEncoder;
        }
        else // davaosetchu
        {
          if (giatriEncoder < 0)
          {
            giatriEncoder = 62;
          }
          else if (giatriEncoder > 62)
          {
            giatriEncoder = 0;
          }
          Name[Setup_3 - 1] = giatriEncoder;
        }
      }
      else if (nhanSetup == 1)
      {
        if (Setup_3 == 6) // thoat
        {
          Setup_3 = 0;
          dongchop = 0;
          for (int i = 0; i < 5; i++)
          {
            if (Nametemp[i] != Name[i])
              resetBoard();
          }
        }
        else // setchu
        {
          if (setchu == 0)
          {
            setchu = 1;
            giatriEncoder = Name[Setup_3 - 1];
          } // vaosetchu
          else // thoatsetchu
          {
            if (Name[Setup_3 - 1] != EEPROM.read(100 + Setup_3))
            {
              EEPROM.write(100 + Setup_3, Name[Setup_3 - 1]);
            }
            setchu = 0;
            Setup_3 = Setup_3 + 1;
            giatriEncoder = Setup_3;
          }
        }
        nhanSetup = 2;
      }

      if (Setup_3 == 6)
      {
        dongchop = 1;
        choptat();
        if (chopchu > 0)
        {
          SetupLCD_3_1();
        }
      }
      else
        SetupLCD_3_1();
    }
    else if (Setup_1 == 3 && Setup_2 == 2) // ID SETUP
    {
      if (nhanSetup == 0)
      {
        if (giatriEncoder <= 0)
        {
          giatriEncoder = 999;
        }
        else if (giatriEncoder >= 1000)
        {
          giatriEncoder = 1;
        }
        IDtam = giatriEncoder;
      }
      else if (nhanSetup == 1)
      {
        if (giatriEncoder != EEPROM.read(106))
        {
          ID = giatriEncoder;
          EEPROM.write(106, ID);
          delay(100);
          resetBoard();
        }
        Setup_3 = 0;
        nhanSetup = 2;
      }
      SetupLCD_3_2();
    }
    else if (Setup_1 == 3 && Setup_2 == 3) // IP SETUP
    {
      if (nhanSetup == 0)
      {
        if (giatriEncoder <= 0)
        {
          giatriEncoder = 2;
        }
        else if (giatriEncoder >= 3)
        {
          giatriEncoder = 1;
        }
        Setup_3 = giatriEncoder;
      }
      else if (nhanSetup == 1)
      {
        IPtinh = Setup_3 - 1;

        if (Setup_3 == 1) // lay ip dong
        {
          if (IPtinh != EEPROM.read(120))
          {
            EEPROM.write(120, IPtinh);
            resetBoard();
          }
          // Setup_3=0;
        }
        else // lay ip tinh
        {
          Setup_4 = 1;
          dongchop = 0;
          giatriEncoder = 1;
          setgiatriIP = 256;
        }
        nhanSetup = 2;
      }
      SetupLCD_3_3();
    }
    else if (Setup_1 == 3 && Setup_2 == 4) // SERVER SETUP
    {
      if (nhanSetup == 0)
      {
        if (giatriEncoder <= 0)
        {
          giatriEncoder = 5;
          dongchop = 1;
        }
        else if (giatriEncoder > 5)
        {
          giatriEncoder = 1;
          dongchop = 0;
        }
        Setup_3 = giatriEncoder;
      }
      else if (nhanSetup == 1)
      {
        if (Setup_3 == 5) // THOAT
        {
          for (int i = 0; i < 32; i++)
          {
            if (ServerAddresstemp[i] != ServerAddress[i])
              resetBoard();
          }
          if (port != mqtt_port)
            resetBoard();

          Setup_3 = 0;
          giatriEncoder = Setup_2;
        }
        else if (Setup_3 == 1) // SERVER ADDRESS
        {
          Setup_4 = 1;
          giatriEncoder = Setup_4;
        }
        else if (Setup_3 == 2) // PORT
        {
          Setup_4 = 1;
          giatriEncoder = port;
        }
        nhanSetup = 2;
      }
      choptat();
      if (chopchu > 0)
      {
        SetupLCD_3_4();
      }
    }
  }
  /////////////////////////////////////////////////////
  else if (Setup_1 > 0 && Setup_2 > 0 && Setup_3 > 0 && Setup_4 > 0) // lop 4
  {
    if (Setup_1 == 3 && Setup_2 == 3 && Setup_3 == 2) // vao SETUP IP tinh
    {
      if (nhanSetup == 0)
      {
        if (setIPtinh == 0)
        {
          if (giatriEncoder <= 0)
          {
            giatriEncoder = 4;
          }
          else if (giatriEncoder >= 5)
          {
            giatriEncoder = 1;
          }
          Setup_4 = giatriEncoder;
        }
        else
        {
          if (setgiatriIP == 256)
          {
            if (giatriEncoder < 1)
            {
              giatriEncoder = 5;
            }
            else if (giatriEncoder > 5)
            {
              giatriEncoder = 1;
            }
            setIPtinh = giatriEncoder;
          }
          else
          {
            if (giatriEncoder < 0)
            {
              giatriEncoder = 255;
            }
            else if (giatriEncoder > 255)
            {
              giatriEncoder = 0;
            }
            setgiatriIP = giatriEncoder;
          }
        }
      }
      else if (nhanSetup == 1)
      {
        if (Setup_4 == 1) // ID ADDRESS
        {
          if (setIPtinh == 0)
          {
            setIPtinh = 1;
            giatriEncoder = setIPtinh;
          } // vao set IP address
          else
          {
            if (setgiatriIP == 256)
            {
              if (setIPtinh == 5) // thoat set IP address
              {
                setIPtinh = 0;
                giatriEncoder = Setup_4;
              }
              else
              {
                setgiatriIP = ipaddress[setIPtinh - 1];
                giatriEncoder = setgiatriIP;
              }
            }
            else
            {
              ipaddress[setIPtinh - 1] = setgiatriIP;
              setIPtinh++;
              giatriEncoder = setIPtinh;
              setgiatriIP = 256;
            }
          }
        }
        else if (Setup_4 == 2) // DEFAULT GATEWAY
        {
          if (setIPtinh == 0)
          {
            setIPtinh = 1;
            giatriEncoder = setIPtinh;
          } // vao set DEFAULT GATEWAY
          else
          {
            if (setgiatriIP == 256)
            {
              if (setIPtinh == 5) // thoat set IP address
              {
                setIPtinh = 0;
                giatriEncoder = Setup_4;
              }
              else
              {
                setgiatriIP = gateway[setIPtinh - 1];
                giatriEncoder = setgiatriIP;
              }
            }
            else
            {
              gateway[setIPtinh - 1] = setgiatriIP;
              setIPtinh++;
              giatriEncoder = setIPtinh;
              setgiatriIP = 256;
            }
          }
        }
        else if (Setup_4 == 3) // SUBNET MASK
        {
          if (setIPtinh == 0)
          {
            setIPtinh = 1;
            giatriEncoder = setIPtinh;
          } // vao set DEFAULT GATEWAY
          else
          {
            if (setgiatriIP == 256)
            {
              if (setIPtinh == 5) // thoat set IP address
              {
                setIPtinh = 0;
                giatriEncoder = Setup_4;
              }
              else
              {
                setgiatriIP = subnet[setIPtinh - 1];
                giatriEncoder = setgiatriIP;
              }
            }
            else
            {
              subnet[setIPtinh - 1] = setgiatriIP;
              setIPtinh++;
              giatriEncoder = setIPtinh;
              setgiatriIP = 256;
            }
          }
        }
        else if (Setup_4 == 4) // thoat set up IP TINH
        {
          Setup_4 = 0;
          giatriEncoder = 2;
          // kiemtra iP tinh co thay doi reset
          int reset = 0;
          if (IPtinh != EEPROM.read(120))
          {
            EEPROM.write(120, IPtinh);
            reset = 1;
          }
          for (int i = 0; i < 4; i++)
          {
            if (ipaddress[i] != EEPROM.read(121 + i))
            {
              EEPROM.write(121 + i, ipaddress[i]);
              reset = 1;
            }
            if (gateway[i] != EEPROM.read(125 + i))
            {
              EEPROM.write(125 + i, gateway[i]);
              reset = 1;
            }
            if (subnet[i] != EEPROM.read(129 + i))
            {
              EEPROM.write(129 + i, subnet[i]);
              reset = 1;
            }
          }
          if (reset == 1)
            resetBoard();
        }
        nhanSetup = 2;
      }
      SetupLCD_3_3_2();
    }
    else if (Setup_1 == 3 && Setup_2 == 4 && Setup_3 == 1) // vao SETUP SERVER ADDRESS
    {
      int vitricokhoangtrang = 0;
      for (int i = 0; i < 32; i++)
      {
        if (ServerAddress[i] == 0)
        {
          vitricokhoangtrang = i + 1;
          i = 32;
        }
      }

      if (nhanSetup == 0)
      {
        if (setchu == 0) // khongsetchu
        {
          if (vitricokhoangtrang == 0) // khong phat hien khoang trang
          {
            if (giatriEncoder <= 0)
            {
              giatriEncoder = 33;
            }
            else if (giatriEncoder > 33)
            {
              giatriEncoder = 1;
            }
            Setup_4 = giatriEncoder;
          }
          else
          {
            if (giatriEncoder <= 0)
            {
              giatriEncoder = 33;
            }
            else if (giatriEncoder > 33)
            {
              giatriEncoder = 1;
            }
            else if ((giatriEncoder < 33) && (giatriEncoder > vitricokhoangtrang))
            {
              if (giatriEncoder > Setup_4)
                giatriEncoder = 33;
              else
                giatriEncoder = vitricokhoangtrang;
            }

            Setup_4 = giatriEncoder;
          }
        }
        else // davaosetchu
        {
          if (giatriEncoder < 0)
          {
            giatriEncoder = 65;
          }
          else if (giatriEncoder > 65)
          {
            giatriEncoder = 0;
          }
          ServerAddress[Setup_4 - 1] = giatriEncoder;
        }
      }
      else if (nhanSetup == 1)
      {
        if (Setup_4 == 33) // thoat
        {
          Setup_4 = 0;
          giatriEncoder = Setup_3; // dongchop=0;
        }
        else // setchu
        {
          if (setchu == 0)
          {
            setchu = 1;
            giatriEncoder = ServerAddress[Setup_4 - 1];
          } // vaosetchu
          else // thoatsetchu
          {
            if (ServerAddress[Setup_4 - 1] == 0) // phat hien khoang trang
            {
              for (int i = Setup_4 - 1; i < 32; i++)
              {
                ServerAddress[i] = 0;
              } // xoa tat ca ky tu phia sau
              Setup_4 = 32;
            }
            if (ServerAddress[Setup_4 - 1] != EEPROM.read(140 + Setup_4))
            {
              EEPROM.write(140 + Setup_4, ServerAddress[Setup_4 - 1]);
            }
            setchu = 0;
            Setup_4 = Setup_4 + 1;
            giatriEncoder = Setup_4;
          }
        }
        nhanSetup = 2;
      }
      if (Setup_4 == 33)
      {
        dongchop = 0;
        choptat();
        if (chopchu > 0)
        {
          SetupLCD_3_4_1();
        }
      }
      else
        SetupLCD_3_4_1();
    }
    else if (Setup_1 == 3 && Setup_2 == 4 && Setup_3 == 2) // vao SETUP PORT
    {
      if (nhanSetup == 0)
      {
        if (giatriEncoder <= 0)
        {
          giatriEncoder = 9999;
        }
        else if (giatriEncoder >= 10000)
        {
          giatriEncoder = 0;
        }
        port = giatriEncoder;
      }
      else if (nhanSetup == 1)
      {
        Setup_4 = 0;
        giatriEncoder = 2;
        nhanSetup = 2;
      }
      SetupLCD_3_4_2();
    }
    else if (Setup_1 == 3 && Setup_2 == 4 && Setup_3 == 3) // vao SETUP USER NAME
    {
    }
    else if (Setup_1 == 3 && Setup_2 == 4 && Setup_3 == 4) // vao SETUP PASSWORD
    {
    }
  }
  Serial.print("Setup:");
  Serial.print(Setup_1);
  Serial.print(".");
  Serial.print(Setup_2);
  Serial.print(".");
  Serial.print(Setup_3);
  Serial.print(".");
  Serial.println(Setup_4);
}
/////////////////////////////////////////////////////////////////////////
void SetupLCD_0()
{

  if (Setup_1 == 1)
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("OUTPUT          ");
    else
      lcd.print("NGO RA          ");
    lcd.setCursor(0, 1);
    if (ngonngu == 0)
      lcd.print("PARAMETER       ");
    else
      lcd.print("THONG SO        ");
  }
  if (Setup_1 == 2)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("OUTPUT          ");
      else
        lcd.print("NGO RA          ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("PARAMETER       ");
      else
        lcd.print("THONG SO        ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("PARAMETER       ");
      else
        lcd.print("THONG SO        ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("SETUP           ");
      else
        lcd.print("CAI DAT         ");
    }
  }
  if (Setup_1 == 3)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("PARAMETER       ");
      else
        lcd.print("THONG SO        ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("SETUP           ");
      else
        lcd.print("CAI DAT         ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("SETUP           ");
      else
        lcd.print("CAI DAT         ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("LANGUAGE        ");
      else
        lcd.print("NGON NGU        ");
    }
  }
  if (Setup_1 == 4)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("SETUP           ");
      else
        lcd.print("CAI DAT         ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("LANGUAGE        ");
      else
        lcd.print("NGON NGU        ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("LANGUAGE        ");
      else
        lcd.print("NGON NGU        ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("EXIT            ");
      else
        lcd.print("THOAT           ");
    }
  }
  if (Setup_1 == 5)
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("LANGUAGE        ");
    else
      lcd.print("NGON NGU        ");
    lcd.setCursor(0, 1);
    if (ngonngu == 0)
      lcd.print("EXIT            ");
    else
      lcd.print("THOAT           ");
  }
}
/////////////////////////////////////////////////////////////////////////
void SetupLCD_1()
{
  if (Setup_2 == 1)
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("IN:  ");
    else
      lcd.print("NGVAO:");

    int dongdienle = (dongdien[0] - (int(dongdien[0]))) * 10;
    sprintf(temp, "%3dV %3d.%dA", dienap[0], int(dongdien[0]), dongdienle);
    lcd.setCursor(5, 0);
    lcd.print(temp);

    sprintf(temp, "%5dW %6dKWh", int(congsuat[0]), int(diennangtieuthu[0] / 1000));
    lcd.setCursor(0, 1);
    lcd.print(temp);
  }
  if (Setup_2 == 10 || Setup_2 == 11)
  {
    if (Setup_2 == 10)
      dongchop = 0;
    else
      dongchop = 1;
    choptat();
    if (chopchu > 0)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("RESET ...       ");
      else
        lcd.print("CAI LAI ...     ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("EXIT            ");
      else
        lcd.print("THOAT           ");
    }
  }
  else
  {
    for (int i = 2; i <= 9; i++)
    {
      if (Setup_2 == i)
      {
        lcd.setCursor(0, 0);
        if (ngonngu == 0)
        {
          lcd.print("OUT");
          lcd.print(i - 1);
          lcd.print(":");
        }
        else
        {
          lcd.print("NGO");
          lcd.print(i - 1);
          lcd.print(":");
        }
        int dongdienle = (dongdien[i - 1] - (int(dongdien[i - 1]))) * 10;
        sprintf(temp, "%3dV %3d.%dA", dienap[i - 1], int(dongdien[i - 1]), dongdienle);
        lcd.setCursor(5, 0);
        lcd.print(temp);

        sprintf(temp, "%5dW %6dKWh", int(congsuat[i - 1]), int(diennangtieuthu[i - 1] / 1000));
        lcd.setCursor(0, 1);
        lcd.print(temp);
      }
    }
  }
}
////////////////////
void SetupLCD_1_10()
{
  if (Setup_3 == 11)
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
      if (ngonngu == 0)
        lcd.print("                ");
      else
        lcd.print("                ");
    }
  }
  else
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("RESET:          ");
    else
      lcd.print("CAI LAI:        ");
    lcd.setCursor(0, 1);
    dongchop = 1;
    choptat();
    if (chopchu > 0)
    {
      if (Setup_3 == 1)
      {
        if (ngonngu == 0)
          lcd.print("ALL             ");
        else
          lcd.print("TAT CA          ");
      }
      else if (Setup_3 == 2)
      {
        if (ngonngu == 0)
          lcd.print("IN              ");
        else
          lcd.print("NGO VAO         ");
      }
      else
      {
        if (ngonngu == 0)
          sprintf(temp, "OUT %d           ", (Setup_3 - 2));
        else
          sprintf(temp, "NGO RA %d        ", (Setup_3 - 2));
        lcd.print(temp);
      }
    }
  }
}
/////////////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////////////////////////////
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
void SetupLCD_3_2() // ID SETUP
{
  if (ngonngu == 0)
    sprintf(temp, "ID:          %3d", IDtam);
  else
    sprintf(temp, "MA:          %3d", IDtam);
  lcd.setCursor(0, 0);
  lcd.print(temp);
  lcd.setCursor(0, 1);
  lcd.print("                ");
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
      sprintf(temp, "%3d.%3d.%3d.%3d ", ipaddress[0], ipaddress[1], ipaddress[2], ipaddress[3]);
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
                sprintf(temp, "%3d", ipaddress[i - 1]);
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
            sprintf(temp, "%3d", ipaddress[i - 1]);
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
/////////////////////////////////////////////////////////////////////////
void SetupLCD_3_4() // SERVER SETUP
{
  if (Setup_3 == 1)
  {
    lcd.setCursor(0, 0);
    if (ngonngu == 0)
      lcd.print("SERVER ADDRESS  ");
    else
      lcd.print("DIA CHI MAY CHU ");
    lcd.setCursor(0, 1);
    if (ngonngu == 0)
      lcd.print("PORT            ");
    else
      lcd.print("CONG QD DU LIEU ");
  }
  if (Setup_3 == 2)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("SERVER ADDRESS  ");
      else
        lcd.print("DIA CHI MAY CHU ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("PORT            ");
      else
        lcd.print("CONG QD DU LIEU ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("PORT            ");
      else
        lcd.print("CONG QD DU LIEU ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("USER NAME       ");
      else
        lcd.print("TEN TAI KHOAN   ");
    }
  }
  if (Setup_3 == 3)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("PORT            ");
      else
        lcd.print("CONG QD DU LIEU ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("USER NAME       ");
      else
        lcd.print("TEN TAI KHOAN   ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("USER NAME       ");
      else
        lcd.print("TEN TAI KHOAN   ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("PASSWORD        ");
      else
        lcd.print("MAT KHAU        ");
    }
  }
  if (Setup_3 == 4)
  {
    if (dongchop == 1)
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("USER NAME       ");
      else
        lcd.print("TEN TAI KHOAN   ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("PASSWORD        ");
      else
        lcd.print("MAT KHAU        ");
    }
    else
    {
      lcd.setCursor(0, 0);
      if (ngonngu == 0)
        lcd.print("PASSWORD        ");
      else
        lcd.print("MAT KHAU        ");
      lcd.setCursor(0, 1);
      if (ngonngu == 0)
        lcd.print("EXIT            ");
      else
        lcd.print("THOAT           ");
    }
  }
  if (Setup_3 == 5)
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
/////////////////////////////////////////////////////////////////////////
void SetupLCD_3_4_1() // SERVER ADDRESS SETUP
{
  if (Setup_4 < 33)
  {
    for (int i = 0; i < 32; i++)
    {
      if (i < 16)
        lcd.setCursor(i, 0);
      else
        lcd.setCursor(i - 16, 1);
      if (i != (Setup_4 - 1))
      {
        lcd.print(chuChar[ServerAddress[i]]);
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
            lcd.print(chuChar[ServerAddress[i]]);
        }
      }
    }
  }
  else
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
/////////////////////////////////////////////////////////////////////////
void SetupLCD_3_4_2() // PORT SETUP
{
  if (ngonngu == 0)
  {
    sprintf(temp, "PORT:       %4d", port);
    lcd.setCursor(0, 0);
    lcd.print(temp);
    lcd.setCursor(0, 1);
    lcd.print("                ");
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("CONG QD DU LIEU:");
    sprintf(temp, "            %4d", port);
    lcd.setCursor(0, 1);
    lcd.print(temp);
  }
}
////////////////////////////////////////////////////////////////////////
void SetupLCD_4()
{
  lcd.setCursor(0, 0);
  if (ngonngu == 0)
    lcd.print("ENGLISH         ");
  else
    lcd.print("TIENG ANH       ");
  lcd.setCursor(0, 1);
  if (ngonngu == 0)
    lcd.print("VIETNAMESE      ");
  else
    lcd.print("TIENG VIET      ");
}
////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////
void docencoder()
{

  if (nhanSetup == 0)
  {
    long now = millis();
    if (pulseIn(A, 1) > 100) // thời gian xung kênh A từ mức cao xuống mức thấp > 50mls
    {
      timer_thoatSetup = now;
      chopchu = 0;
      if (digitalRead(B) == 1)
      {
        giatriEncoder++;
        dongchop = 1;
      }
      if (digitalRead(B) == 0)
      {
        giatriEncoder--;
        dongchop = 0;
      }
      // Serial.println(giatriEncoder);
    }
    else
    {
      if (now - timer_thoatSetup > 20000)
      {
        timer_thoatSetup = now;
        Setup_1 = 0;
        Setup_2 = 0;
        Setup_3 = 0;
        Setup_4 = 0;
        lcd.clear();
      }
    }
  }
  /////////////////doc nut
  int nut = digitalRead(OK);
  if (nut == 0 && nhanSetup == 0)
  {
    nhanSetup = 1;
    long now = millis();
    timer_thoatSetup = now;
  } // Serial.println("OK");
  else if (nut == 1)
  {
    nhanSetup = 0;
  } // Serial.println("nha");
}
/////////////////////////////////////////////////////////////////////////////////////////////
void docEEPROM()
{

  //" aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789.-";
  //           10        20        30        40        50        60
  //"192.168.11.48";
  /*
  EEPROM.write(100,0);//ngon ngu tieng anh
  //NAME
  EEPROM.write(101,32);//P
  EEPROM.write(102,30);//O
  EEPROM.write(103,46);//W
  EEPROM.write(104,10);//E
  EEPROM.write(105,36);//R

  EEPROM.write(106,1);//ID:1

  EEPROM.write(120,0);//IP:dong
  //ipaddress 192.168.10.254
  EEPROM.write(121,192);EEPROM.write(122,168);EEPROM.write(123,10);EEPROM.write(124,254);
  //gateway 192.168.10.1
  EEPROM.write(125,192);EEPROM.write(126,168);EEPROM.write(127,10);EEPROM.write(128,1);
  //subnet 255.255.255.0
  EEPROM.write(129,255);EEPROM.write(130,255);EEPROM.write(131,255);EEPROM.write(132,0);

  //SERVER address
  EEPROM.write(141,54);//1
  EEPROM.write(142,62);//9
  EEPROM.write(143,55);//2
  EEPROM.write(144,63);//.
  EEPROM.write(145,54);//1
  EEPROM.write(146,59);//6
  EEPROM.write(147,61);//8
  EEPROM.write(148,63);//.
  EEPROM.write(149,54);//1
  EEPROM.write(150,54);//1
  EEPROM.write(151,63);//.
  EEPROM.write(152,57);//4
  EEPROM.write(153,61);//8
  for(int i=14;i<33;i++){EEPROM.write(140+i,0);}//" "
  */
  EEPROM.write(173, 0X07); // PORT 1884;0x075C
  EEPROM.write(174, 0X5C);

  //////////////////////////////////////////////////////////////////////////////////////////
  ngonngu = EEPROM.read(100);

  for (int i = 0; i < 5; i++)
  {
    Name[i] = EEPROM.read(101 + i);
    Nametemp[i] = Name[i];
  }

  ID = EEPROM.read(106);

  IPtinh = EEPROM.read(120);

  for (int i = 0; i < 4; i++)
  {
    ipaddress[i] = EEPROM.read(121 + i);
  }
  for (int i = 0; i < 4; i++)
  {
    gateway[i] = EEPROM.read(125 + i);
  }
  for (int i = 0; i < 4; i++)
  {
    subnet[i] = EEPROM.read(129 + i);
  }

  for (int i = 0; i < 32; i++)
  {
    ServerAddress[i] = EEPROM.read(141 + i);
    ServerAddresstemp[i] = ServerAddress[i];
  }

  portH = EEPROM.read(173);
  portL = EEPROM.read(174);
}
//////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{
  if (ketnoimang == 0)
  {
    ConnectEthernet();
  }
  else
  {
    if (!mqttClient.connected())
    {
      ketnoiserver = 0;
      reconnect();
    }
    ethernet();
  }
  if (lenh == 1)
  {
    long now = millis();
    timer_ketnoiserver = now + timerketnoiserver;
    // if(khoidong==0 )dieukhienrelay();//&& khongcheAuto==0
    dieukhienrelay();
    lenh = 0;
    khoidong = 0;
  }
  doctrangthai();
  if (ketnoiserver == 1)
    guitrangthai();

  // int nut=digitalRead(OK);
  // if(nut==0 && nhanSetup==0 && Setup_1==0){Setup_1=1;lcd.clear();giatriEncoder=1;dongchop=0;nhanSetup=2;long now = millis();timer_thoatSetup = now;}
  // else if(nut==1){nhanSetup=0;}

  if (Setup_1 > 0)
  {
    SetupLCD();
  }
  else
  {
    hienLCD();
  }
  // docaserial();
  // docencoder();
}
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////