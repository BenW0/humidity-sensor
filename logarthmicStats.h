// Creates a cascading list of mean and max datapoints 
// to provide high-fidelity short-term statistics as well
// as low-fidelity long-term statistics.

#pragma once

#include <TimeLib.h>
#include "util.h"
#include <string>

// Number of steps to store in each layer of the logorthmic tracker
#define STATS_STEP_SIZE 3
//#define NUMBER_OF_STAT_LAYERS 10

struct SampleType
{

  float temp;
  float humidity;

  void PrintOut() const
  {
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.print(" Humidity: ");
    Serial.print(humidity);
  }

  String AsString() const
  {
    return "Temp = " + String(temp) + "; Humidity = " + String(humidity);
  }

  void KeepMax(SampleType const & other)
  {
    temp = max(temp, other.temp);
    humidity = max(humidity, other.humidity);
  }

  void KeepMin(SampleType const &other)
  {
    temp = min(temp, other.temp);
    humidity = min(humidity, other.humidity);
  }

  SampleType operator+(SampleType const & other) const
  {
    return {temp + other.temp, humidity + other.humidity};
  }

  void operator+=(SampleType const & other)
  {
      temp += other.temp;
      humidity += other.humidity;
  }

  SampleType operator*(float other) const
  {
    return {temp * other, humidity * other};
  }

  SampleType operator/(float denom) const
  {
    return {temp / denom, humidity / denom};
  }
};
const SampleType ZeroSample{0., 0.};
const SampleType SmallSample{-200, -1};
const SampleType LargeSample{2000, 101};

// Needs to be implemented by the main program; called when stats should be pushed by a layer
void pushStats(SampleType const & sample_min, SampleType const & sample_max, SampleType const & sample_mean, time_t timestamp, uint16_t bad_samples);

struct LogStatsEntry
{
  time_t epoch;
  SampleType minValue;
  SampleType meanValue;
  SampleType maxValue;

  void PrintOut()
  {
    Serial.print(" Time: ");
    PrintTime(epoch);
    Serial.print(" >> Min: [");
    minValue.PrintOut();
    Serial.print("] Mean: [");
    meanValue.PrintOut();
    Serial.print("] Max: [");
    maxValue.PrintOut();
    Serial.println("]");
  }
};

struct LogStatsTracker
{
  uint8_t sample_count = 0;
  uint16_t bad_sample_count = 0;
  SampleType sample_sum = ZeroSample;
  SampleType sample_max = SmallSample;
  SampleType sample_min = LargeSample;
  time_t last_epoch = 0;
  bool am_full = false;

  static const uint8_t agg_interval = 10;

  bool push_stats = false;

  LogStatsEntry hist[STATS_STEP_SIZE];
  uint8_t hist_count = 0;
  uint8_t layer = 0;

  LogStatsTracker *next = nullptr;
  LogStatsTracker *prev = nullptr;

  void Log(SampleType value, time_t epoch)
  {
    Log(value, LargeSample, SmallSample, epoch);
  }

  void LogBadSamples(uint16_t count )
  {
    bad_sample_count += count;
  }

  void Log(SampleType value, SampleType minValue, SampleType maxValue, time_t epoch)
  {
    sample_sum += value;
    sample_min.KeepMin(value);
    sample_min.KeepMin(minValue);
    sample_max.KeepMax(value);
    sample_max.KeepMax(maxValue);
    sample_count++;
    last_epoch = epoch;
    if (sample_count > agg_interval)
    {
      SampleType mean = GetCurrentMean();
      if (push_stats)
        pushStats(sample_min, sample_max, mean, epoch, bad_sample_count);
      if (hist_count >= STATS_STEP_SIZE)
      {
        hist_count--;
        if (next != nullptr)
        {
          next->Log(mean, sample_min, sample_max, epoch);
          next->LogBadSamples(bad_sample_count);
        }
      }
      for (int16_t i = hist_count; i > 0; --i)
        hist[i] = hist[i - 1];
      hist[0] = {epoch, sample_min, mean, sample_max};
      hist_count++;

      sample_count = 0;
      sample_sum = ZeroSample;
      sample_max = SmallSample;
      sample_min = LargeSample;
      bad_sample_count = 0;
    }
  }

  SampleType GetCurrentMean()
  {
    if (sample_count > 0)
      return sample_sum / float(sample_count);
    return ZeroSample;
  }

  SampleType GetHistMean()
  {
    if (GetHistSampleCount() == 0)
      return ZeroSample;
    SampleType sum = ZeroSample;
    for (uint8_t i = 0; i < hist_count; ++i)
      sum += hist[i].meanValue;
    sum += sample_sum * sample_count / agg_interval;
    if (prev)
      sum += prev->GetHistMean() * prev->GetHistSampleCount() / agg_interval;
    return sum / GetHistSampleCount();
  }

  SampleType GetHistMax()
  {
    SampleType maxValue = SmallSample;
    for (uint8_t i = 0; i < hist_count; ++i)
      maxValue.KeepMax(hist[i].maxValue);
    if (prev)
      maxValue.KeepMax(prev->GetHistMax());
    return maxValue;
  }

  SampleType GetHistMin()
  {
    SampleType minValue = LargeSample;
    for (uint8_t i = 0; i < hist_count; ++i)
      minValue.KeepMin(hist[i].minValue);
    if (prev)
      minValue.KeepMin(prev->GetHistMin());
    return minValue;
  }

  float GetHistSampleCount()
  {
    return hist_count + sample_count / agg_interval + prev->GetHistSampleCount() / agg_interval;
  }

  time_t GetOldestTime()
  {
    if (hist_count == 0)
      return last_epoch;
    return hist[hist_count - 1].epoch;
  }

  void PrintOut()
  {
    Serial.print("Log Layer ");
    Serial.print(layer);
    Serial.print(". n=");
    Serial.print(hist_count);
    Serial.print(" mean=");
    GetHistMean().PrintOut();
    Serial.print(" max=");
    GetHistMax().PrintOut();
    Serial.println("Historical data:");
    for (uint8_t i = 0; i < hist_count; ++i)
    {
      hist[i].PrintOut();
    }
  }

  String GetStatsString()
  {
    time_t epoch = GetOldestTime();
    if(epoch == 0)
      return "";  // no stats have been collected yet
    return "Since " + StrTime(epoch) + " n = " + String(GetHistSampleCount()) + " Max = " + GetHistMax().AsString() + " Mean = " + GetHistMean().AsString() + "\n";
    
  }
};
