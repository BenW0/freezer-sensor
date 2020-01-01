
/*
 * Freezer Sensor arduino app.
 * 
 * Compiles with Arduino 1.8.10 and the following libraries
 *   - ESP8266 core 2.5.4
 *      this creates an issue for the UniversalTelegramBot, see below
 *   - OneWire 2.3.4
 *   - DallasTemperature 3.8.0
 *   - NTPClient 3.2.0
 *   - HTTPSRedirect commit eceabbe (https://github.com/electronicsguy/ESP8266)
 *  I also had Teensyduino 1.4.8 installed, but I'm not sure it's required to run this sketch.
 * 
 *  BearSSL doesn't work for the UniversalTelegramBot and HTTPSRedirect. In addition to changes
 *  below, the HTTPSRedirect file needs to be updated to use axTLS instead by adding two lines after
 *  the #pragma once:
 *    #include <WiFiClientSecureAxTLS.h>
 *    using namespace axTLS;
 * 
 *  If cloning fresh from the repo, create a secrets.h file which contains the following:
 *  
  const char* ssid     = "***";
  const char* password = "***";
  

  #define BOTtoken "***"  // your Bot Token (Get from Botfather) on Telegram

 */

#define USING_AXTLS
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define TESTING

#include "secrets.h"
#include "util.h"
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include "googleLogging.h"
#include "telegram.h"
#include "logarthmicStats.h"


const float SAMPLES_PER_HOUR = 60. * 60.;
unsigned long constexpr MS_BETWEEN_SAMPLES = 1000 * 60. * 60. / SAMPLES_PER_HOUR;

#define ONE_WIRE_BUS 2
#define BAD_TEMP DEVICE_DISCONNECTED_F

// Number of steps to store in each layer of the logorthmic tracker
#define STATS_STEP_SIZE 3
#define NUMBER_OF_STAT_LAYERS 10

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
const unsigned long timeOffset = -8 * 60 * 60;  // time zone offset

WiFiClientSecure client;
TelegramIO telegram(client);

GoogleLogging glog;

unsigned long nextUpdateTime = 0;
uint32_t totalBadSamples = 0;
uint32_t totalGoodSamples = 0;

LogStatsTracker trackers[NUMBER_OF_STAT_LAYERS];  // static initialization of the arrays makes memory usage easier to track at compile time

void setup(void) 
{  
  Serial.begin(115200); 
  Serial.setTimeout(2000);
  
  while(!Serial) { }
  Serial.println("Freezer Sensor"); 

  // Init DS18B20
  sensors.begin(); 
  
  // Init wifi
  WiFi.hostname("FreezerTempSensor");
  WiFi.begin(ssid, password);
  Serial.println("Wifi init.");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  timeClient.begin();
  timeClient.setUpdateInterval(60000*60);
  timeClient.update();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  delay(2000);

  glog.setup();

  // init the stats trackers
  for(uint8_t i = 0; i < NUMBER_OF_STAT_LAYERS - 1; ++i)
    {
      trackers[i].next = &trackers[i+1];
      trackers[i+1].prev = &trackers[i];
      trackers[i].layer = i;
    }
  trackers[1].push_stats = true;

  nextUpdateTime = millis();
} 

float getTemp()
{
#ifdef TESTING
  static uint32_t inc = 0;
  return inc++;
#endif
  sensors.requestTemperatures(); // Send the command to get temperature readings 
  return sensors.getTempFByIndex(0);
}

void pushStats(float mean_temp, float max_temp, time_t epoch)
{
  Serial.println("PushStats");
  glog.postData(StrTime(epoch), mean_temp, max_temp);
}

String getStatsString()
{
  String s;
  for (uint8_t i = 0; i < NUMBER_OF_STAT_LAYERS; ++i)
    s += trackers[i].GetStatsString();
  return s;
}

void loop(void) 
{ 
  // catch clock rollover
  if (millis() < nextUpdateTime && nextUpdateTime - millis() > MS_BETWEEN_SAMPLES * 10)
  {
    telegram.update();
    delay(1000);
    return;
  }

  timeClient.update();
  time_t time = timeClient.getEpochTime();

  float temp = getTemp();
  if (temp != BAD_TEMP)
  {
    totalGoodSamples++;
  
    // update the logarthmic stats tracker
    trackers[0].Log(temp, time);
  }
  else
  {
    totalBadSamples++;
  }

  // temp: print trackers
  Serial.print("Read: ");
  Serial.println(temp);
  Serial.print(" Totals: Good: ");
  Serial.print(totalGoodSamples);
  Serial.print(" Bad: ");
  Serial.println(totalBadSamples);
  for(uint8_t i = 0; i < NUMBER_OF_STAT_LAYERS; ++i)
  {
    Serial.print(i);
    Serial.print(" - ");
    trackers[i].PrintOut();
    Serial.println();
  }
  
  nextUpdateTime = nextUpdateTime + MS_BETWEEN_SAMPLES;
} 
