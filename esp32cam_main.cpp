//camera transmitting code. sends a 240x240 RGB565 image over UDP in chunks, which currently results in high latency and packet losses. not good :(

#include <Arduino.h>
#include <esp_camera.h>
#include <WiFi.h>
#include <WiFiUdp.h>

WiFiUDP udp;



const uint8_t esp32s3Address[] = {0x34,0x85,0x18,0x9b,0x5c,0x30}; //redundant, left just in case

//these were also used for single pixel testing
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F

#define debugLEDpin 12

// server definition
const char *ssid = "ESP32S3";
const char *password = "BIGCAMERA864";
const uint16_t portUDP = 1235;
const IPAddress IP(192, 168, 4, 1);
const char* IP_char = "192.168.4.1";


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

//redundant, old struct for one-pixel packets
typedef struct _imagepacket
{
  uint8_t identifier;
  uint16_t pixel;
} _imagepacket;

#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22





void setup() {

  //camera init
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;

  config.frame_size = FRAMESIZE_240X240;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  Serial.begin(115200);
  Serial.println("Started.");
  pinMode(debugLEDpin,OUTPUT);

  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid,password);
  WiFi.setSleep(0);

  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(debugLEDpin, 1);
    delay(50);
    digitalWrite(debugLEDpin, 0);
    delay(950);
  }
  
  udp.begin(portUDP);
  Serial.println("Connected.");

  if (esp_camera_init(&config)!= ESP_OK) {
    Serial.println("Camera init failed.");
    digitalWrite(debugLEDpin,1);
    while (1);
  }
  digitalWrite(debugLEDpin,0);
  Serial.println("Setup finished, running loop in 500ms");
  delay(500);
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  uint16_t *image565 = (uint16_t *)fb->buf;
  
  imagepacket currentChunk = {0};
  int identifierCounter = 0;
  int currentChunkCounter = 0 ;
  for (int i=0;i<fb->width*fb->height;i++) {  //parses each pixel
    currentChunk.identifier=identifierCounter;
    currentChunk.imageChunk[currentChunkCounter]=image565[i];
    currentChunkCounter++;
    if (currentChunkCounter>=pixelsPerChunk) {
      currentChunkCounter = 0;
      identifierCounter++;
      udp.beginPacket(IP_char,portUDP);
      udp.write((uint8_t *)&currentChunk,sizeof(currentChunk));
      udp.endPacket();
    }
  }
  if (currentChunkCounter>0) {
    for (int i=currentChunkCounter;i<pixelsPerChunk;i++) {
      currentChunk.imageChunk[i] = 0x0000; //writing a blank pixel just to prevent errors
    }
    udp.beginPacket(IP_char,portUDP);
    udp.write((uint8_t *)&currentChunk,sizeof(currentChunk));
    udp.endPacket();
   }
   
  esp_camera_fb_return(fb);



  
  //was used for one pixel testing
  
  /*sendstruct.identifier = 0;
  sendstruct.pixel = RED;
  udp.beginPacket(IP_char,portUDP);
  udp.write((uint8_t *)&sendstruct,sizeof(sendstruct));
  udp.endPacket();
  interrupts();
  Serial.println("Sent RED");
  delay(1000);
  sendstruct.identifier = 1;
  sendstruct.pixel = GREEN;
  udp.beginPacket(IP_char,portUDP);
  udp.write((uint8_t *)&sendstruct,sizeof(sendstruct));
  udp.endPacket();
  interrupts();
  Serial.println("Sent GREEN");
  delay(1000);
  sendstruct.identifier = 2;
  sendstruct.pixel = BLUE;
  udp.beginPacket(IP_char,portUDP);
  udp.write((uint8_t *)&sendstruct,sizeof(sendstruct));
  udp.endPacket();
  Serial.println("Sent BLUE");
  delay(1000);*/

}
