// Creates a cascading list of mean and max datapoints 
// to provide high-fidelity short-term statistics as well
// as low-fidelity long-term statistics.

#pragma once

#include <TimeLib.h>
#include "util.h"
#include <string>

// Number of steps to store in each layer of the logorthmic tracker
#define STATS_STEP_SIZE 3
#define NUMBER_OF_STAT_LAYERS 10

// Needs to be implemented by the main program; called when stats should be pushed by a layer
void pushStats(float sample_max, float sample_mean, time_t timestamp);

struct LogStatsEntry
{
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

struct LogStatsTracker
{
  uint8_t sample_count = 0;
  float sample_sum = 0.;
  float sample_max = -200.;
  time_t last_epoch = 0;

  static const uint8_t agg_interval = 2;

  bool push_stats = false;

  LogStatsEntry hist[STATS_STEP_SIZE];
  uint8_t hist_count = 0;
  uint8_t layer = 0;

  LogStatsTracker *next = nullptr;
  LogStatsTracker *prev = nullptr;

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
    last_epoch = epoch;
    if (sample_count > agg_interval)
    {
      float mean = GetCurrentMean();
      if (push_stats)
        pushStats(sample_max, mean, epoch);
      if (hist_count >= STATS_STEP_SIZE)
      {
        hist_count--;
        if (next != nullptr)
          next->Log(mean, sample_max, epoch);
      }
      for (int16_t i = hist_count; i > 0; ++i)
        hist[i] = hist[i - 1];
      hist[0] = {epoch, mean, sample_max};
      hist_count++;

      sample_count = 0;
      sample_sum = 0.;
      sample_max = -200.;
    }
  }

  float GetCurrentMean()
  {
    if (sample_count > 0)
      return sample_sum / sample_count;
    return 0.;
  }

  float GetHistMean()
  {
    if (hist_count == 0)
      return 0.;
    float sum = 0.;
    for (uint8_t i = hist_count; i > 0; ++i)
      sum += hist[i].meanTemp;
    sum += sample_sum * sample_count / agg_interval;
    if (prev)
      sum += prev->GetHistMean() * prev->GetHistSampleCount() / agg_interval;
    return sum / GetHistSampleCount();
  }

  float GetHistMax()
  {
    if (hist_count == 0)
      return 0.;
    float maxTemp = -1000;
    for (uint8_t i = hist_count; i > 0; ++i)
      maxTemp = max(hist[i].maxTemp, maxTemp);
    if (prev)
      maxTemp = max(maxTemp, prev->GetHistMax());
    return maxTemp;
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
    Serial.print(GetHistMean());
    Serial.print(" max=");
    Serial.println(GetHistMax());
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
    return "Since " + StrTime(epoch) + " n = " + String(GetHistSampleCount()) + " Max = " + String(GetHistMax()) + " Mean = " + String(GetHistMean()) + "\n";
    
  }
};