//the main station/remote code. it has an ST7789V3 240x280 IPS display, control stick and an encoder for actuator control. Launches a wifi AP that esp32c3 and esp32cam connect to and receive/send data

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>


//not currently used.
IPAddress esp32camIP;


// server definition
const char *ssid = "ESP32S3";
const char *password = "BIGCAMERA864";
const uint16_t portTCP = 1234;
const uint16_t portUDP = 1235;

//TCP is used for controls
WiFiServer server(portTCP);

//UDP is used for video streaming
WiFiUDP udp;

// display definition
TFT_eSPI display = TFT_eSPI();

// CONFIG
int display_FPS = 60;


// DISPLAY PINS
#define TFT_CS 5    // Chip select control pin
#define TFT_RST 10  // Reset pin (could connect to RST pin)
#define TFT_DC 16   // Data Command control pin
#define TFT_SCLK 11 // Clock pin
#define TFT_MOSI 12 // Data out pin
#define TFT_MISO -1
#define TOUCH_CS -1

// ENCODER PINS
#define ENC_LEFT 13
#define ENC_RIGHT 14

// CONTROL STICK PINS
#define AxisX_Pin 1
#define AxisY_Pin 2

int deadZoneBoundaries = 109; // in both directions from half, zone where control stick input would be taken as 0

byte invertAxisX = 1; // 0-no, 1-yes
byte invertAxisY = 1; // 0-no, 1-yes

//redundant, ingnore these.
const uint8_t myAddress[] = {0x34, 0x85, 0x18, 0x9b, 0x5c, 0x30};
const uint8_t esp32c3Address[] = {0x94, 0xA9, 0x90, 0x96, 0x61, 0x5C};


WiFiClient unknownClient0;
WiFiClient unknownClient1;

WiFiClient esp32c3Client;
WiFiClient esp32camClient;

esp_now_peer_info_t esp32c3peerInfo;


typedef struct controlpacket
{
  int8_t pitch;
  int8_t roll;
  int8_t power;
} controlpacket;


//IMAGE CONFIG
const int imageWidth = 240;
const int imageHeight = 240;
const int pixelsPerChunk = 730;

const int imageSize = imageWidth*imageHeight;

typedef struct imagepacket
{
  uint8_t identifier;
  uint16_t imageChunk[pixelsPerChunk];
} imagepacket;

//this struct was used for single pixel UDP testing
typedef struct _imagepacket
{
  uint8_t identifier;
  uint16_t pixel;
} _imagepacket;
// encoder interrupts definition

// allegedly has better performance than digitalRead and others
#define READ_GPIO_FAST(pin) ((GPIO.in >> (pin)) & 0x1) 

volatile byte encleft_flag = 0;
volatile byte encright_flag = 0;
volatile int8_t encmoved = 0; // 0 idle;-1 left;1 right
int counter = 0;
unsigned long _lastEncRotationMillis = 0;
byte _ENC_DEBOUNCE_TIME = 10; // in ms, or us if using micros
byte update_display = 0;

// TEST FLAGS
byte foundESP32C3, staetofUNKNCLNT, stateofESP32C3CLNT, UNCT0CONNECTED, UNCT0AVAILABLE, CLIENTAMOUNT; //debug flags
uint8_t ___ENC_CHECKTIME()
{
  if (millis() - _lastEncRotationMillis >= _ENC_DEBOUNCE_TIME)
  {
    _lastEncRotationMillis = millis();
    return 1;
  }
  else
    return 0;
}
void IRAM_ATTR _ENC_LEFT_INTERRUPT()
{
  // uint8_t state = digitalRead(ENC_LEFT);
  uint8_t state = READ_GPIO_FAST(ENC_LEFT);
  if (state)
  {
    encleft_flag = 0;
  }
  else
  {
    encleft_flag = 1;
    if (encright_flag)
    {
      encmoved = -1;
      encright_flag = 0;
      encleft_flag = 0;
    }
  }
}
void IRAM_ATTR _ENC_RIGHT_INTERRUPT()
{
  // uint8_t state = digitalRead(ENC_RIGHT);
  uint8_t state = READ_GPIO_FAST(ENC_RIGHT);
  if (state)
  {
    encright_flag = 0;
  }
  else
  {
    encright_flag = 1;
    if (encleft_flag)
    {
      encmoved = 1;
      encleft_flag = 0;
      encright_flag = 0;
    }
  }
}


void setup()
{
  // display init
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  display.init();
  display.fillScreen(TFT_BLACK);
  display.setCursor(5, 5);
  display.setTextSize(2);
  display.setTextColor(TFT_GREEN);
  display.println("Hello.");
  display.setTextColor(TFT_RED);
  display.println("very very red hellio");
  display.setTextColor(TFT_BLUE);
  display.println("now a very blue hellio");
  display.setTextColor(TFT_WHITE);
  display.setTextSize(1);



  // wifi
  WiFi.softAP(ssid, password);
  WiFi.setSleep(0);
  server.begin();
  server.setNoDelay(1);

  udp.begin(portUDP);
  


  // encoder, control stick init
  pinMode(ENC_LEFT, INPUT_PULLUP);
  pinMode(ENC_RIGHT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_LEFT), _ENC_LEFT_INTERRUPT, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_RIGHT), _ENC_RIGHT_INTERRUPT, CHANGE);

  pinMode(AxisX_Pin, INPUT);
  pinMode(AxisY_Pin, INPUT);

}

unsigned long lastMillis = 0;
int framedeltatime = 1000 / display_FPS;

uint16_t imageBuffer[imageSize];
byte updateImage;


void loop()
{


  //HARDWARE
  // encoder reading
  if (encmoved)
  {
    if (___ENC_CHECKTIME())
    {
      noInterrupts();
      counter += encmoved;
      if (counter < 0)
      {
        counter = 0;
      }
      if (counter > 100)
      {
        counter = 100;
      }
      encmoved = 0;
      if (update_display)
      {
        display.fillScreen(TFT_BLACK);
        display.setCursor(0, 0);
        display.print(counter);
      }

      interrupts();
    }
  }
  
  
  
  
  
  // WIRELESS PROTOCOL
  WiFiClient newClient = server.available();
  if (newClient&&CLIENTAMOUNT<2)
  {
    if (unknownClient0)
    {
      unknownClient1 = newClient;
      CLIENTAMOUNT = 2;
    }
    else
    {
      unknownClient0 = newClient;
      CLIENTAMOUNT = 1;
    }
  }
  UNCT0CONNECTED = unknownClient0.connected();
  UNCT0AVAILABLE = unknownClient0.available();

  //WiFi TCP for ESP32C3
  if ((unknownClient0 ? 1 : 0) && unknownClient0.connected() && unknownClient0.available())
  {
    String initmsg = unknownClient0.readStringUntil('\n');
    initmsg.trim();
    if (initmsg.startsWith("ESP32CAM"))
    {
      //ignore this, was previously planning on using TCP but switched to UDP for esp32cam
    }
    else if (initmsg.startsWith("ESP32C3"))
    {
      esp32c3Client = unknownClient0;
      foundESP32C3 = 1;
      esp32c3Client.println("Connected.");
      while (esp32c3Client.available() > 0)
        esp32c3Client.read();
    }
  }

  if (unknownClient1 && unknownClient1.connected() && unknownClient1.available())
  {
    String initmsg = unknownClient1.readStringUntil('\n');
    initmsg.trim();
    if (initmsg.startsWith("ESP32CAM"))
    {
      //ignore this, was previously planning on using TCP but switched to UDP for esp32cam
    }
    else if (initmsg.startsWith("ESP32C3"))
    {
      esp32c3Client = unknownClient1;
      esp32c3Client.println("Connected.");
      foundESP32C3 = 1;
      while (esp32c3Client.available() > 0)
        esp32c3Client.read();
    }
  }

  //WiFi UDP for ESP32CAM

  //old testing code for singel pixel tests
  /*if (udp.parsePacket()) {
    int packetSize = sizeof(_imagepacket);
    char buffer[packetSize];
    int length=udp.read(buffer,packetSize);
    if (length==packetSize) {
      
      memcpy(&imageChunk,buffer,packetSize);
      imageBuffer = imageChunk.pixel;
      updateImage = 1;
    }
  }*/

  if (udp.parsePacket()) {
    int structSize = sizeof(imagepacket);
    char buffer[structSize];
    int length = udp.read(buffer,structSize);
    if (length==structSize) {
      imagepacket imageChunk;
      memcpy(&imageChunk,buffer,structSize);
      unsigned int initialIndex = imageChunk.identifier*pixelsPerChunk;
      unsigned int maxIndex = initialIndex+pixelsPerChunk-1;
      //if (initialIndex!=0) initialIndex--;
      updateImage = 1;
      for (int i=initialIndex;i<=maxIndex;i++) {
        if (i>=imageSize) {updateImage=1;break;}
        imageBuffer[i]=imageChunk.imageChunk[i-initialIndex];
      }
    }
  }
  

  

  //DISPLAY
  // display update
  if (millis() - lastMillis >= framedeltatime)
  {
    display.setTextSize(1);
    display.fillRect(0, 240, 239, 279, TFT_BLACK);
    display.setCursor(0, 240);
    display.setTextColor(TFT_CYAN);
    int rawReadX, rawReadY;
    int8_t pitch, roll;
    if (esp32c3Client && esp32c3Client.connected())
    {
      rawReadY = analogRead(AxisY_Pin);
      rawReadX = analogRead(AxisX_Pin);
      if (rawReadY >= 2048 - deadZoneBoundaries && rawReadY <= 2048 + deadZoneBoundaries)
      {
        pitch = 0;
      }
      else
      {
        pitch = map(rawReadY, 0, 4095, -5, 5) * (2 - invertAxisX * 2 - 1);
      }

      if (rawReadX >= 2048 - deadZoneBoundaries && rawReadX <= 2048 + deadZoneBoundaries)
      {
        roll = 0;
      }
      else
      {
        roll = map(rawReadX, 0, 4095, -5, 5) * (2 - invertAxisY * 2 - 1);
      }
      controlpacket outputdata;
      outputdata.pitch = pitch;
      outputdata.roll = roll;
      outputdata.power = counter;
      int bytesSent = esp32c3Client.write((uint8_t *)&outputdata, sizeof(outputdata));
      display.printf("Pitch:%d, Roll:%d, Power%d\n", pitch, roll, counter);
      display.println();
      display.print(".  bytes sent:");
      display.print(bytesSent);
      display.print("; expected:");
      display.print(sizeof(outputdata));
      
    }
    else
    {
      display.print("NOCON");
      display.print("found:");
      display.print(foundESP32C3);
      display.print(";UNKNCLNT:");
      display.print(unknownClient0 ? 1 : 0);
      display.print("; C3CLNT:");
      display.print(esp32c3Client ? 1 : 0);
      display.print("; CNCTD:");
      display.print(UNCT0CONNECTED);
      display.print("; AVLBL:");
      display.print(UNCT0AVAILABLE);
      display.print("CLS:");
      display.print(CLIENTAMOUNT);
      

      display.println(";");
    }



    if (updateImage) {

      display.pushImage(0,0,imageWidth,imageHeight,imageBuffer);

      updateImage = 0;
    }

    lastMillis = millis();
  }

}
