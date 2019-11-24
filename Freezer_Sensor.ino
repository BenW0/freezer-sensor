
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
  
  const char APIKey[ ]      = "***";      //My maker key for Webhooks on IFTTT
 */
#include "secrets.h"
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <NTPClient.h>
#include <TimeLib.h>

#define USING_AXTLS
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "BensWebserver.h"

const float SAMPLES_PER_HOUR = 60.;
unsigned long constexpr MS_BETWEEN_SAMPLES = 1000 * 60. * 60. / SAMPLES_PER_HOUR;

#define ONE_WIRE_BUS 2
#define BAD_TEMP DEVICE_DISCONNECTED_F

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
const unsigned long timeOffset = -8 * 60 * 60;  // time zone offset
unsigned long nextUpdateTime = 0;

// webserver
void webCallback(WiFiClient & client);
BensWebserver webserver(webCallback);

// historical data
#define HIST_SIZE 4096
struct Hist_t {
  float temp;
  time_t epoch;
};
Hist_t hist_temps[HIST_SIZE];
Hist_t *hist_ptr = hist_temps;

void pushStats(float max_temp, float mean_temp, float min_temp);

struct StatsTracker {
  uint32_t sample_count = 0;
  uint32_t good_sample_count = 0;
  float sample_sum = 0.;
  float sample_max = -200.;
  float sample_min = 200.;
  float sample_last = 0.;
  uint32_t reset_interval = 1000;

  bool push_stats = false;

  StatsTracker(uint32_t interval, bool pushStats = false) : reset_interval(interval), push_stats(pushStats)
  {}
  
  void Log(float value)
  {
    sample_count++;
    if (value != BAD_TEMP)
    {
      sample_last = value;
      good_sample_count++;
      sample_sum += value;
      sample_max = max(sample_max, value);
      sample_min = min(sample_min, value);
    }
    if (sample_count > reset_interval)
    {
      if (push_stats)
        pushStats(sample_max, GetMean(), sample_min);
      sample_count = 0;
      good_sample_count = 0;
      sample_sum = 0.;
      sample_max = -200.;
      sample_min = 200.;
    }
  }

  float GetMean()
  {
    if (good_sample_count > 0)
      return sample_sum / good_sample_count;
    return 0.;
  }

  void PrintOut(WiFiClient & client)
  {
    client.print("<p>Measurements: ");
    client.print(sample_count);
    client.print(" (");
    client.print(sample_count - good_sample_count);
    client.println(")</p>");

    client.print("<p>Average: ");
    client.print(GetMean());
    client.print(" F (");
    client.print(sample_min);
    client.print("F - ");
    client.print(sample_max);
    client.println("F)</p>");
  }

  void PrintOut()
  {
    Serial.print(" Temperature: "); 
    Serial.print(sample_last); 
    Serial.print(" Count: ");
    Serial.print(sample_count);
    Serial.print(" (");
    Serial.print(sample_count - good_sample_count);
    Serial.print(" bad) Min: ");
    Serial.print(sample_min);
    Serial.print(" Max: ");
    Serial.print(sample_max);
    Serial.print(" Mean: ");
    Serial.println(sample_sum / good_sample_count);
  }
};

StatsTracker daily(SAMPLES_PER_HOUR * 24, true);
StatsTracker weekly(SAMPLES_PER_HOUR * 24 * 7);
StatsTracker monthly(SAMPLES_PER_HOUR * 24 * 30);

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

  webserver.begin();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  delay(2000);

  
  // TESTING
  pushStats(1, 2, 3);
  

  // clear temperature history
  for(size_t i = 0; i < HIST_SIZE; ++i)
    hist_temps[i].epoch = 0;

  nextUpdateTime = millis();
} 

float getTemp()
{
  sensors.requestTemperatures(); // Send the command to get temperature readings 
  return sensors.getTempFByIndex(0);
}

void pushIFTTT(const String & trigger, const String & arglist)
{
  const char host[ ]        = "maker.ifttt.com";          // maker channel of IFTTT
  Serial.print("Connect to: ");
  Serial.println(host);
  // WiFiClient to make HTTP connection
  WiFiClient client;
   if (!client.connect(host, 80)) {
    Serial.println("connection failed");
    return;
    }

  // Build URL
  String url = String("/trigger/") + trigger + String("/with/key/") + APIKey + arglist;
  Serial.print("Requesting URL: ");
  Serial.println(url);

  // Send request to IFTTT
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n"); 
  delay(20);

  // Read all the lines of the reply from server and print them to Serial
  Serial.println("Respond:");
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
    }
  Serial.println();
  Serial.println("closing connection");
  Serial.println();
  Serial.println();
  client.stop();
}

void pushAlert(float max_temp, float mean_temp, float minutes)
{
  pushIFTTT("freezer_alert", String("?MaxTemp=") + max_temp + String("&MeanTemp=") + mean_temp + String("&Minutes=") + minutes);
}

void pushStats(float max_temp, float mean_temp, float min_temp)
{
  pushIFTTT("freezer_stats", String("?MaxTemp=") + max_temp + String("&MeanTemp=") + mean_temp + String("&GoodSamples=") + min_temp);
}

// callback for the web server; print out the body of the html
void webCallback(WiFiClient & client)
{
  client.println("<h2>Daily</h2>");
  daily.PrintOut(client);
  client.println("<h2>Weekly</h2>");
  weekly.PrintOut(client);
  client.println("<h2>Monthly</h2>");
  monthly.PrintOut(client);
  
  auto handleEntry = [&](Hist_t * ptr)
  {
    if (ptr->epoch > 0)
    {
      TimeElements tm;
      breakTime(ptr->epoch, tm);
      client.print("<tr><td>");
      client.print(tm.Year - 30 + 2000);
      client.print("-");
      client.print(tm.Month);
      client.print("-");
      client.print(tm.Day);
      client.print(" ");
      client.print(tm.Hour);
      client.print(":");
      client.print(tm.Minute);
      client.print(":");
      client.print(tm.Second);

      client.print(" </td><td>");
      client.print(ptr->temp);
      client.println("</td></tr>");
    }
  };

  client.println("<h2>History</h2>");
  client.println("<table><tr><th>Timestamp</th><th>Temp (F)</th></tr>");
  // start with the oldest entry
  for(Hist_t * ptr = hist_ptr; ptr < hist_temps + HIST_SIZE; ++ptr)
  {
    handleEntry(ptr);
  }
  for(Hist_t * ptr = hist_temps; ptr < hist_ptr; ++ptr)
  {
    handleEntry(ptr);
  }
  client.print("</table>");
}

void loop(void) 
{ 
  // catch clock rollover
  if (millis() < nextUpdateTime && nextUpdateTime - millis() > MS_BETWEEN_SAMPLES * 10)
  {
    webserver.process();
    delay(1000);
    return;
  }

  timeClient.update();
  float t = getTemp();

  // add the temp to the hist data
  *hist_ptr++ = {t, timeClient.getEpochTime() + timeOffset};
  if(hist_ptr - hist_temps >= HIST_SIZE)
    hist_ptr = hist_temps;

  // update the weekly stats
  daily.Log(t);
  weekly.Log(t);
  monthly.Log(t);

  Serial.print(WiFi.localIP());
  weekly.PrintOut();

  nextUpdateTime = nextUpdateTime + MS_BETWEEN_SAMPLES;
  
   
} 
