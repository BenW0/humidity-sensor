
/*
 * Freezer Sensor arduino app.
 * 
 * Compiles with Arduino 1.8.10 and the following libraries
 *   - ESP8266 core 2.5.4
 *      this creates an issue for the UniversalTelegramBot, see below
 *   - Adafruit BusIO v 1.7.2
 *   - Adafruit SHTC Library v 1.0.0
 *   - Adafruit Unified Sensor v 1.1.4
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
  const char *GScriptId = "...";
 */

#define USING_AXTLS
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define TESTING

#include "secrets.h"
#include "util.h"
#include "Adafruit_SHTC3.h"
#include <NTPClient.h>
#include <TimeLib.h>
#include "googleLogging.h"
#include "logarthmicStats.h"

#define SCRIPT_VERSION "0.1"
#define DEVICE_NAME "Hornet"

const float SAMPLES_PER_HOUR = 60. * 60;
unsigned long constexpr MS_BETWEEN_SAMPLES = 1000 * 60. * 60. / SAMPLES_PER_HOUR;

#define ONE_WIRE_BUS 2
#define BAD_TEMP DEVICE_DISCONNECTED_F

// Number of steps to store in each layer of the logorthmic tracker
#define STATS_STEP_SIZE 3
#define NUMBER_OF_STAT_LAYERS 10

Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
const unsigned long timeOffset = -8 * 60 * 60;  // time zone offset

WiFiClientSecure client;

GoogleLogging glog;

unsigned long nextUpdateTime = 0;
uint32_t totalBadSamples = 0;
uint32_t totalGoodSamples = 0;

LogStatsTracker tracker;

void setup(void) 
{  
  Serial.begin(115200); 
  Serial.setTimeout(2000);
  
  while(!Serial) { }
  Serial.println("Humidity Sensor"); 
  Serial.print("Ver ");
  Serial.println(SCRIPT_VERSION);

  // Init the sensor
  if (!shtc3.begin())
  {
    Serial.println("Couldn't find SHTC3");
  }

    // Init wifi
    WiFi.hostname("HumidityTempSensor");
    WiFi.begin(ssid, password);
    Serial.println("Wifi init.");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(true);
    Serial.println();

    timeClient.begin();
    timeClient.setUpdateInterval(60000 * 60);
    timeClient.update();

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
    delay(2000);

    glog.setup();

    // init the stats trackers
    /*for(uint8_t i = 0; i < NUMBER_OF_STAT_LAYERS - 1; ++i)
    {
      trackers[i].next = &trackers[i+1];
      trackers[i+1].prev = &trackers[i];
      trackers[i].layer = i;
    }*/
    tracker.push_stats = true;

    nextUpdateTime = millis();
  }

bool getSensorValues(float & tempOut, float & humidityOut)
{
#ifdef TESTING
  static uint32_t inc = 0;
  tempOut = inc;
  humidityOut = inc+10;
  return true;
#endif
  sensors_event_t humidity, temp;

  shtc3.reset();
  delay(10);

  bool success = shtc3.getEvent(&humidity, &temp); // populate temp and humidity objects with fresh data

  tempOut = temp.temperature;
  humidityOut = humidity.relative_humidity;
  return success;
}

void pushStats(SampleType const & mean_sample, SampleType const & min_sample, SampleType const & max_sample, time_t epoch, uint16_t bad_samples)
{
  Serial.println("PushStats");
  String arguments = String("name=") + DEVICE_NAME + "date=" + StrTime(epoch) + "&badValues=" + String(bad_samples);
  arguments += "minTemp" + String(min_sample.temp) + "&meanTemp=" + String(mean_sample.temp) + "&maxTemp=" + String(max_sample.temp);
  arguments += "minHumidity" + String(min_sample.humidity) + "&meanHumidity=" + String(mean_sample.humidity) + "&maxHumidity=" + String(max_sample.humidity);
  //glog.postData(arguments);
}

void loop(void) 
{ 
  // catch clock rollover
  if (millis() < nextUpdateTime && nextUpdateTime - millis() > MS_BETWEEN_SAMPLES * 10)
  {
    //telegram.update();
    delay(1000);
    return;
  }

  timeClient.update();
  time_t time = timeClient.getEpochTime();

  float temp = 0, humidity = -1;
  bool success = getSensorValues(temp, humidity);
  if (success)
  {
    totalGoodSamples++;
  
    // update the logarthmic stats tracker
    tracker.Log({temp, humidity}, time);

    // temp: print trackers
    Serial.print("Read: ");
    Serial.print(temp);
    Serial.print(" ");
    Serial.println(humidity);
    Serial.print(" Totals: Good: ");
    Serial.print(totalGoodSamples);
    Serial.print(" Bad: ");
    Serial.println(totalBadSamples);

    tracker.PrintOut();
    Serial.println();
  }
  else
  {
    tracker.LogBadSamples(1);
    totalBadSamples++;
    Serial.println("Bad sample");
  }

  
  nextUpdateTime = nextUpdateTime + MS_BETWEEN_SAMPLES;
} 
