
#include <string>

#ifndef SIMPLE_STATS_BUFFER_SIZE
#define SIMPLE_STATS_BUFFER_SIZE 128
#endif

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

  void KeepMax(SampleType const &other)
  {
    temp = max(temp, other.temp);
    humidity = max(humidity, other.humidity);
  }

  void KeepMin(SampleType const &other)
  {
    temp = min(temp, other.temp);
    humidity = min(humidity, other.humidity);
  }

  SampleType operator+(SampleType const &other) const
  {
    return {temp + other.temp, humidity + other.humidity};
  }

  void operator+=(SampleType const &other)
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

class SimpleStats
{
public:
  using callback_t = void(*)(const SampleType&, const SampleType&, const SampleType&, const uint16_t&);

  SimpleStats(callback_t callback_) : callback(callback_)
  {
    count = 0;
  }

  void Log(SampleType sample)
  {
    buffer[count++] = sample;
    if (count == SIMPLE_STATS_BUFFER_SIZE - 1)
    {
      callback(GetAverage(), GetMin(), GetMax(), badSamples);
      count = 0;
      badSamples = 0;
    }
  }

  void LogBadSample()
  {
    badSamples++;
  }

  SampleType GetMin() const
  {
    SampleType minVal = LargeSample;
    for(uint16_t i = 0; i < count; ++i)
    {
      minVal.KeepMin(buffer[i]);
    }
    return minVal;
  }

  SampleType GetMax() const
  {
    SampleType maxVal = SmallSample;
    for (uint16_t i = 0; i < count; ++i)
    {
      maxVal.KeepMax(buffer[i]);
    }
    return maxVal;
  }

  SampleType GetAverage() const
  {
    SampleType val = ZeroSample;
    if (count == 0)
      return val;
    for (uint16_t i = 0; i < count; ++i)
    {
      val += buffer[i];
    }
    return val / count;
  }

private : 
  callback_t callback;
  SampleType buffer[SIMPLE_STATS_BUFFER_SIZE];
  uint16_t count{0};
  uint16_t badSamples{0};
};
