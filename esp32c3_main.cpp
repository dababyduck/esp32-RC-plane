//esp32-c3 actuator controller. uses ledc to control two sg90 servos in a stabilator configuration. receives angle info over WiFi TCP.

//btw, some variables/functions might be redundant/overcomplicated for no reason, so if you see that you can point that out

#include <Arduino.h>

#include <WiFi.h>



// server definition
const char *ssid = "ESP32S3";
const char *password = "BIGCAMERA864";
const uint16_t portTCP = 1234;
const IPAddress IP(192, 168, 4, 1);

// ANGLE CONFIG
byte invertRoll = 1;  // invert Roll.   0 - no, 1 - yes
byte invertPitch = 0; // invert Pitch.  0 - no, 1 - yes

#define pitchMaxAngle 30     // pitch max angle in each direction
#define rollMaxAngle 30      // roll max angle in each direction
#define clampingMagnitude 45 // used for clamping too large angles as actuators become inefficient with too high of angles, like 60

// PINS
#define servo0pin 3 // left servo
#define servo1pin 4 // right servo
#define powerPin 5  // main motor
#define debugLEDpin 0
#define debugLEDpin2 2

// SERVO CONFIG

#define servoMinPercent 2.5 // 1ms
#define servoMaxPercent 12  // 2ms
#define servoFreq 50
#define servoresolution 12
#define minAngle -90
#define maxAngle 90

uint32_t servoMin = 111;
uint32_t servoMax = 491;

long mypow(int num, int power)
{
  // FYI 0^0=1.
  if (power == 0)
    return 1;
  long result = 1;
  for (int i = 1; i <= power; i++)
  {
    result *= num;
  }
  return result;
}

uint32_t resMaxVal;

const uint8_t esp32s3Address[] = {0x34, 0x85, 0x18, 0x9b, 0x5c, 0x30};

/// Servo servo1;
typedef struct controlpacket
{
  int8_t pitch;
  int8_t roll;
  int8_t power;
} controlpacket;

controlpacket inputdata;

void setServoAngle(byte channel, int8_t angle)
{
  uint32_t duty = map(angle,
                      minAngle, maxAngle,
                      (resMaxVal * servoMinPercent) / 100, (resMaxVal * servoMaxPercent) / 100);
  ledcWrite(channel, duty);
}

WiFiClient me;

void flush()
{
  while (me.available() > 0)
  {
    me.read();
  }
}
byte ledflag, PASSEDSTATUS;



void setup()
{ 
  resMaxVal = mypow(2, servoresolution) - 1;

  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(0);
  me.setNoDelay(1);
  pinMode(debugLEDpin, OUTPUT);
  pinMode(debugLEDpin2,OUTPUT);
  
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(debugLEDpin, 1);
    delay(50);
    digitalWrite(debugLEDpin, 0);
    delay(950);
  }
  PASSEDSTATUS = 1;

  while (!me.connected())
  {
    if (me.connect(IP, portTCP))
    {
      digitalWrite(debugLEDpin, 0);
    }
    else
    {
      digitalWrite(debugLEDpin, 1);
    }
    delay(1000);
  }

  PASSEDSTATUS = 2;
  while (me.available() <= 0)
  {
    digitalWrite(debugLEDpin,1);
    me.print("ESP32C3\n");
    delay(250);
    digitalWrite(debugLEDpin,0);
    delay(250);
  }
  PASSEDSTATUS = 3;
  flush();
  PASSEDSTATUS = 4;

  // my servo boiz
  ledcSetup(0, 50, servoresolution);
  ledcSetup(1, 50, servoresolution);
  ledcAttachPin(servo0pin, 0);
  ledcAttachPin(servo1pin, 1);
}



void loop()
{
  PASSEDSTATUS = 5;
  if (me.connected() && me.available())
  {
    me.read((uint8_t *)&inputdata, sizeof(inputdata));
    flush();
    PASSEDSTATUS = 6;
    int8_t pitchAngle = map(inputdata.pitch, -5, 5, -pitchMaxAngle, pitchMaxAngle);
    int8_t rollAngle = map(inputdata.roll, -5, 5, -rollMaxAngle, rollMaxAngle);
    int8_t totalAngle0 = rollAngle * (invertRoll ? -1 : 1) + pitchAngle * (invertPitch ? -1 : 1);
    int8_t totalAngle1 = rollAngle * (invertRoll ? -1 : 1) - pitchAngle * (invertPitch ? -1 : 1);
    totalAngle0 = constrain(totalAngle0, -clampingMagnitude, clampingMagnitude);
    totalAngle1 = constrain(totalAngle1, -clampingMagnitude, clampingMagnitude);

    setServoAngle(0, totalAngle0);
    setServoAngle(1, totalAngle1);

  } 

}
