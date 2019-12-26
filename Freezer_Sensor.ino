
/*
 * Freezer Sensor arduino app.
 * 
 * Compiles with Arduino 1.8.10 and the following libraries
 *   - ESP8266 core 2.5.4
 *      this creates an issue for the UniversalTelegramBot
 *   - OneWire 2.3.4
 *   - DallasTemperature 3.8.0
 *   - NTPClient 3.2.0
 *  I also had Teensyduino 1.4.8 installed, but I'm not sure it's required to run this sketch.
 *  
 *  If cloning fresh from the repo, create a secrets.h file which contains the following:
 *  
  const char* ssid     = "***";
  const char* password = "***";
  

  #define BOTtoken "***"  // your Bot Token (Get from Botfather) on Telegram

 */
#include "secrets.h"
#include "util.h"
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <NTPClient.h>
#include <TimeLib.h>

#define USING_AXTLS
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define TESTING

const float SAMPLES_PER_HOUR = 60. * 60.;
unsigned long constexpr MS_BETWEEN_SAMPLES = 1000 * 60. * 60. / SAMPLES_PER_HOUR;

#define ONE_WIRE_BUS 2
#define BAD_TEMP DEVICE_DISCONNECTED_F

// Number of steps to store in each layer of the logorthmic tracker
#define STATS_STEP_SIZE 3
#define NUMBER_OF_STAT_LAYERS 10

void pushStats(float sample_max, float sample_mean, time_t timestamp);

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
const unsigned long timeOffset = -8 * 60 * 60;  // time zone offset

unsigned long nextUpdateTime = 0;
uint32_t total_bad_samples = 0;
uint32_t total_good_samples = 0;

struct LogStatsEntry {
  time_t epoch;
  float meanTemp;
  float maxTemp;

  void PrintOut()
  {
    Serial.print(" Time: ");
    PrintTime(epoch);
    Serial.print(" Mean Temp: "); 
    Serial.print(meanTemp); 
    Serial.print(" Max: ");
    Serial.println(maxTemp);
  }
};

struct LogStatsTracker {
  uint8_t sample_count = 0;
  float sample_sum = 0.;
  float sample_max = -200.;

  static const uint8_t agg_interval = 2;
  
  bool push_stats = false;

  LogStatsEntry hist[STATS_STEP_SIZE];
  uint8_t hist_count = 0;

  LogStatsTracker *next = nullptr;

  void Log(float value, time_t epoch)
  {
    Log(value, -200, epoch);
  }
  
  void Log(float value, float maxValue, time_t epoch)
  {
    sample_sum += value;
    sample_max = max(sample_max, value);
    sample_max = max(sample_max, maxValue);
    sample_count++;
    if (sample_count > agg_interval)
    {
      float mean = GetMean();
      if (push_stats)
        pushStats(sample_max, mean, epoch);
      if(hist_count >= STATS_STEP_SIZE)
      {
        hist_count--;
        if(next != nullptr)
          next->Log(mean, sample_max, epoch);
      }
      for(int8_t i = hist_count; i > 0; ++i)
        hist[i] = hist[i-1];
      hist[0] = {epoch, mean, sample_max};
      hist_count++;
      
      sample_count = 0;
      sample_sum = 0.;
      sample_max = -200.;
    }
  }

  float GetMean()
  {
    if (sample_count > 0)
      return sample_sum / sample_count;
    return 0.;
  }

  void PrintOut()
  {
    for(uint8_t i = 0; i < hist_count; ++i)
    {
      hist[i].PrintOut();
    }
  }
};

LogStatsTracker trackers[NUMBER_OF_STAT_LAYERS];

void setup(void) 
{  
  Serial.begin(115200); 
  Serial.setTimeout(2000);
  
  while(!Serial) { }
  Serial.println("Freezer Sensor"); 

  // Init DS18B20
  sensors.begin(); 
  
  // Init wifi
  WiFi.hostname("ESP8266TempSensor"); //This changes the hostname of the ESP8266 to display neatly on the network esp on router.
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

  // init the stats trackers
  for(uint8_t i = 0; i < NUMBER_OF_STAT_LAYERS - 1; ++i)
      trackers[i].next = &trackers[i+1];
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

void pushStats(float mean_temp, float min_temp, time_t epoch)
{
  Serial.println("PushStats");
}

void loop(void) 
{ 
  // catch clock rollover
  if (millis() < nextUpdateTime && nextUpdateTime - millis() > MS_BETWEEN_SAMPLES * 10)
  {
    delay(1000);
    return;
  }

  timeClient.update();
  float t = getTemp();
  time_t time = timeClient.getEpochTime();
  
  if (t != BAD_TEMP)
  {
    total_good_samples++;
  
    // update the logarthmic stats tracker
    trackers[0].Log(t, time);
  }
  else
  {
    total_bad_samples++;
  }

  // temp: print trackers
  Serial.print(" Totals: Good: ");
  Serial.print(total_good_samples);
  Serial.print(" Bad: ");
  Serial.println(total_bad_samples);
  for(uint8_t i = 0; i < NUMBER_OF_STAT_LAYERS; ++i)
  {
    Serial.print(i);
    Serial.print(" - ");
    trackers[i].PrintOut();
    Serial.println();
  }
  
  nextUpdateTime = nextUpdateTime + MS_BETWEEN_SAMPLES;
  
   
} 
