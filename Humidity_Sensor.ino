
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

//#define TESTING
// #define TESTING_MOCK_SENSOR

#include "secrets.h"
#include "util.h"
#include "Adafruit_SHTC3.h"
#include <NTPClient.h>
#include <TimeLib.h>
#include "googleLogging.h"
#include "simpleStats.h"
#include "fifo_queue.h"

#define SCRIPT_VERSION "0.2"
#define DEVICE_NAME "Hornet"

const float UPLOADS_PER_HOUR = 1;
const float SAMPLES_PER_HOUR = UPLOADS_PER_HOUR * SIMPLE_STATS_BUFFER_SIZE;
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


unsigned long nextUpdateTime = 0;
uint32_t totalBadSamples = 0;
uint32_t totalGoodSamples = 0;

// Define a fifo queue for 
struct SummaryToSend
{
  SampleType mean_sample, min_sample, max_sample;
  uint16_t bad_samples;
  time_t time_stamp;
};

#define SEND_QUEUE_LENGTH 72
FifoQueue<SummaryToSend, SEND_QUEUE_LENGTH> sendQueue;


void bufferFull(SampleType const &mean_sample, SampleType const &min_sample, SampleType const &max_sample, const uint16_t& bad_samples);

SimpleStats tracker(bufferFull);

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
      for (uint8_t retry = 0; retry < 40 && WiFi.status() != WL_CONNECTED; ++retry)
      {
        delay(500);
        Serial.print(".");
      }
      if (WiFi.status() != WL_CONNECTED)
      {
        WiFi.begin(ssid2, password2);
        Serial.println("Wifi init, key 2.");
        for (uint8_t retry = 0; retry < 40 && WiFi.status() != WL_CONNECTED; ++retry)
        {
          delay(500);
          Serial.print(".");
        }
        if (WiFi.status() != WL_CONNECTED)
        {
          WiFi.begin(ssid3, password3);
          Serial.println("Wifi init, key 3.");
          for (uint8_t retry = 0; retry < 40 && WiFi.status() != WL_CONNECTED; ++retry)
          {
            delay(500);
            Serial.print(".");
          }
        }
      }
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

    nextUpdateTime = millis() + MS_BETWEEN_SAMPLES;
    
    //SampleType test{99, 100};
    //bufferFull(test, test, test, 3);
  }

bool getSensorValues(float & tempOut, float & humidityOut)
{
#ifdef TESTING_MOCK_SENSOR
  static uint32_t inc = 0;
  tempOut = inc;
  humidityOut = inc+10;
  return true;
#endif
  sensors_event_t humidity, temp;

  shtc3.reset();
  delay(10);

  bool success = shtc3.getEvent(&humidity, &temp); // populate temp and humidity objects with fresh data

  tempOut = temp.temperature * 9.0 / 5.0 + 32.0;
  humidityOut = humidity.relative_humidity;
  return success;
}

void bufferFull(SampleType const & mean_sample, SampleType const & min_sample, SampleType const & max_sample, const uint16_t& bad_samples)
{

  time_t epoch = timeClient.getEpochTime() + timeOffset;
  SummaryToSend summary;
  summary.time_stamp = epoch;
  summary.mean_sample = mean_sample;
  summary.min_sample = min_sample;
  summary.max_sample = max_sample;
  summary.bad_samples = bad_samples;
  sendQueue.Push(summary);
}

void pushStats()
{
  SummaryToSend summary;
  if (sendQueue.Peek(summary))
  {
    Serial.println("Sending stats");
    String arguments = String("name=") + DEVICE_NAME + "&date=" + StrTime(summary.time_stamp, true) + "&badValues=" + String(summary.bad_samples);
    arguments += "&minTemp=" + String(summary.min_sample.temp) + "&meanTemp=" + String(summary.mean_sample.temp) + "&maxTemp=" + String(summary.max_sample.temp);
    arguments += "&minHumidity=" + String(summary.min_sample.humidity) + "&meanHumidity=" + String(summary.mean_sample.humidity) + "&maxHumidity=" + String(summary.max_sample.humidity);

    Serial.println(arguments);

    GoogleLogging glog;
    glog.setup();
    if (glog.postData(arguments))
    {
      sendQueue.Pop(summary);
      Serial.println("Post successful");
    }
    else
    {
      Serial.println("Post failed");
    }
  }
}

void loop(void) 
{ 
  // catch clock rollover
  {
    unsigned long curTime = millis();
    if (curTime < nextUpdateTime && nextUpdateTime - curTime > MS_BETWEEN_SAMPLES * 10)
    {
      delay(min(nextUpdateTime - curTime, MS_BETWEEN_SAMPLES) / 8);
      return;
    }
  }

   timeClient.update();

   float temp = 0, humidity = -1;
   bool success = getSensorValues(temp, humidity);
  if (success)
  {
    totalGoodSamples++;
  
    // update the logarthmic stats tracker
    tracker.Log({temp, humidity});

    // temp: print trackers
    Serial.print("Read: ");
    Serial.print(temp);
    Serial.print(" ");
    Serial.println(humidity);
    Serial.print(" Totals: Good: ");
    Serial.print(totalGoodSamples);
    Serial.print(" Bad: ");
    Serial.println(totalBadSamples);

    //tracker.PrintOut();
    Serial.println();
  }
  else
  {
    tracker.LogBadSample();
    totalBadSamples++;
    Serial.println("Bad sample");
  }

  pushStats();

  if (nextUpdateTime + MS_BETWEEN_SAMPLES < millis())
  {
     nextUpdateTime = millis();
     Serial.println("Next update time already reached; resetting");
  }
  nextUpdateTime = nextUpdateTime + MS_BETWEEN_SAMPLES;
} 
